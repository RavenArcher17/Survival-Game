#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Modelled hand tools, built as one runtime mesh each (no engine shapes): a lathe-turned haft with an oval section,
 * a taper and a knob at the butt; a knapped stone head (flat-shaded facets, a chipped edge, a ground bit) for the
 * hatchet; a curved double-pointed head for the pick; criss-cross rawhide lashing binding the head on; a fishing rod
 * (tapering wand, wrapped grip, a spool of wound line, guide rings); the rod's carved float.
 *
 * Space: X from the grip (the fist at 0) toward the head, Y toward the striking side (where the fingers point), Z the
 * flat of the head. Vertex colour carries the surface (RGB) and roughness (A) for M_HeldTool, which adds grain along
 * U (UVs run U along a part's length).
 */
struct WILDREACH_API FWRToolMesh
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;

	/** What to build: "hatchet" / "pick" / "rod" (Kind), crude or stone (bCrude), its head colour (linear). */
	static FWRToolMesh Tool(FName Kind, bool bCrude, const FLinearColor& Head, float Length, float Back, float Thick, float HeadScale);
	/** A weapon of a class beyond the sword (dagger, spear, greatsword, great_hammer, scythe) in its metal. */
	static FWRToolMesh Weapon(FName Class, const FLinearColor& Metal, float Scale = 1.f);
	/** The fishing float, centred on its waterline: the red cap up +Z. */
	static FWRToolMesh Float();
	/** Where the rod's tip sits in tool space (the line leaves from there). */
	static FVector RodTip(float Length, float Back);

	/** Puts the mesh in a procedural mesh component's section 0 with M_HeldTool (or a vertex-colour-free fallback). */
	void ApplyTo(UProceduralMeshComponent* Comp) const;
	static UMaterialInterface* Material();

	// Builders (public for the fishing line and future tools).
	/** A turned part along Axis from Start: Profile = (t 0..1 along Length, radius across Y, radius across Z). */
	void Lathe(const FVector& Start, const FVector& Axis, float Length, const TArray<FVector>& Profile, int32 Sides, const FLinearColor& Colour, float Rough, float Bend = 0.f, float Seed = 0.f);
	/** A tube following Points (a helix, a ring), radius R. */
	void Tube(const TArray<FVector>& Points, float R, int32 Sides, const FLinearColor& Colour, float Rough, bool bClosed = false);
	/** A closed convex-ish solid lofted through cross-section rings (each ring the same vertex count), flat-shaded. */
	void Loft(const TArray<TArray<FVector>>& Rings, const FLinearColor& Colour, float Rough, bool bCapEnds = true);

private:
	void Tri(const FVector& A, const FVector& B, const FVector& C, const FColor& Col, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC);
	static FColor Pack(const FLinearColor& Colour, float Rough);
	void HatchetHead(const FVector& At, float S, bool bCrude, const FLinearColor& Head);
	void PickHead(const FVector& At, float S, bool bCrude, const FLinearColor& Head);
	void Lashing(const FVector& At, float HaftR, float Span, bool bCrude, bool bCross);
};
