#pragma once

#include "CoreMinimal.h"
#include "WRTreeKit.h"

/**
 * The Ancient forest drawn with Epic's Electric Dreams scans (Aaron 11-16, q-electric-dreams: "Yes do the better forest";
 * cooks/wild-answers-1116.md 4.1). Data/forest_art.json (Tools/art/electric_dreams_forest.py writes it) says, per forest
 * dressing kind, which scans draw it, how each is fitted, and the collision capsules measured off each scan. The
 * collision comes from the data, never from a loaded mesh, so a server or a PC without the kit builds the same walls as
 * one with it; without the kit (Tools/art/electric_dreams_forest.ps1 not run) the kind draws its tree-kit look.
 */
namespace WRForestArt
{
	struct FMesh
	{
		FString Path;
		/** Bounds height and base (cm, the mesh's own). */
		float Height = 100.f;
		float MinZ = 0.f;
		/** Capsules in mesh space (WRTreeKit's collider shape). */
		TArray<FWRTreeCollider> Colliders;
		/** Material slot -> material path (the hornbeams' leaf colour per canopy species). */
		TMap<int32, FString> Materials;
	};
	struct FKind
	{
		TArray<FMesh> Meshes;
		float Cull = 0.f;
		bool bShadow = false;
		/** A share of the height pressed into the ground (Electric Dreams' embed). */
		float Sink = 0.f;
		/** true: scaled to the height the generator gave the instance (at most MaxFit x the mesh); false: the mesh's own
		 *  size x the instance's scale (the ground layer). */
		bool bFitToKit = true;
		float MaxFit = 0.f;
	};

	/** The row for a dressing kind, or null (that kind draws as before). */
	WILDREACH_API const FKind* Find(const FString& Kind);

	/** An instance's drawn transform: Ground is where the generator stood it, Yaw its turn, Scale its scale, KitHeight the
	 *  tree kit's reference height for its species (cm; unused for the ground layer). Normal tilts it onto the slope
	 *  (the ground layer; Z-up for the rest). */
	WILDREACH_API FTransform Place(const FKind& K, const FMesh& M, const FVector& Ground, float Yaw, float Scale, float KitHeight, const FVector& Normal = FVector::UpVector);
}
