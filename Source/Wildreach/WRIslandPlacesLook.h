// The Fire and Frost places' builders (private to WRIslandPlaces*.cpp): one place being built, in its own frame (local
// X its facing, Z up, the origin on the ground at its centre), and the kinds' looks (WRIslandPlacesLook.cpp,
// WRIslandPlacesLook2.cpp, WRIslandPlacesKit.cpp).
#pragma once

#include "CoreMinimal.h"
#include "WRBuildLookKit.h"
#include "WRIslandPlaces.h"

class UWRIslandPlaces;
class AWRIslandPlacesActor;
class UStaticMesh;
class UMaterialInterface;

struct FWRIsleSite
{
	FWRIsleSite(UWRIslandPlaces* InSys, AWRIslandPlacesActor* InOwner, const FTransform& InWorld, bool bInDraw, int32 InKey);

	UWRIslandPlaces* Sys = nullptr;
	AWRIslandPlacesActor* Owner = nullptr;
	USceneComponent* Root = nullptr;
	FTransform World;
	TArray<FWRLookBlock> Blocks;
	WRLookKit::FKit K;
	bool bDraw = true;
	int32 Key = 0;
	int32 Variant = 0;
	TSharedPtr<FJsonObject> Row;
	int32 HazardCount = 0;

	/** The ground's height (cm) at local (X, Y), relative to the origin (traced once per metre and remembered). */
	float Gz(float X, float Y);
	FVector G(float X, float Y, float Up = 0.f) { return FVector(X, Y, Gz(X, Y) + Up); }
	/** World point -> local, and back. */
	FVector Local(const FVector& WorldAt) const { return World.InverseTransformPosition(WorldAt); }
	FVector ToWorld(const FVector& LocalAt) const { return World.TransformPosition(LocalAt); }
	/** Seeded 0..1 / -1..1 from a local point and a salt (the same on every machine). */
	float R(const FVector& At, int32 Salt) const { return K.Rand(At, Salt); }
	float J(const FVector& At, int32 Salt) const { return K.Jit(At, Salt); }

	/** Collision you walk on or are stopped by (a box, local). */
	class UBoxComponent* Solid(const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator);
	/** A light that's on at night only (eased by the clock), or always (bAlways). */
	class UPointLightComponent* Lamp(const FVector& At, const FLinearColor& Colour, float Intensity, float Radius, bool bAlways = false);
	/** A looping fx row attached here (always on), or one the clock starts at dusk and stops at dawn. */
	USceneComponent* Fx(const FVector& At, FName FxRow, float Scale = 1.f);
	void NightFx(const FVector& At, FName FxRow, float Scale = 1.f);
	/** A breathing vent: a light and an fx burst every Period s (offset Phase), Length s long (the Bellows, hornitos). */
	void Breath(const FVector& At, FName FxRow, float Period, float Phase, float Length, float Intensity, float Radius, const FLinearColor& Colour);
	/** A waymark's ember cup: lit for you for a while when you pass it at night. */
	void PassCup(const FVector& At, const FLinearColor& Colour, float Intensity, float Radius);
	/** A brazier lit while a shrine is lit (the Slag Light). */
	void ShrineLight(const FVector& At, FName Shrine, float Intensity, float Radius);
	/** A pulsing glowmoth light (night). */
	void Glowmoth(const FVector& At, float Phase);
	/** A pack rock (SimpleRocks) fitted to Size, its foot at Base (local), in a tinted rock material. */
	void Rock(int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, const FLinearColor& Tint, bool bCollide, float Sink = 10.f);
	/** A tower or block of glacier ice (Tools/art/make_meshes.py's seracs in M_IceBlock: snow on its faces turned up, all
	 *  snow with Snow 1) fitted to Size, its foot at Base (local). Where the mesh isn't there, a look-kit block. */
	UStaticMeshComponent* Ice(int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, bool bCollide, float Sink = 40.f, float Snow = 0.f);
	/** Instances of a pack mesh (pines, char trees, shrubs) at local transforms (scale fits Height cm tall). */
	void Meshes(const TCHAR* Path, const TArray<FTransform>& Local, float Height, float Cull, const FLinearColor* Coat = nullptr);
	/** A hazard of this place (key = the place's key + its count). */
	FWRIsleHazard& Hazard(FWRIsleHazard::EKind Kind, const FVector& LocalAt, float Radius);
	/** Finish: the blocks become instanced shapes culled at Cull cm. */
	void Done(float Cull);

private:
	TMap<FIntPoint, float> GroundCache;
	TMap<FString, UMaterialInterface*> RockMats;
};

namespace WRIsleLook
{
	/** Build a kind at the site; false for a kind this build doesn't know. */
	bool Build(FWRIsleSite& S, const FString& Kind);
	bool BuildVolcanic(FWRIsleSite& S, const FString& Kind);
	bool BuildTundra(FWRIsleSite& S, const FString& Kind);
	bool BuildKit(FWRIsleSite& S, const FString& Kind);
	/** sRGB bytes -> linear. */
	inline FLinearColor Lin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	/** A local point on the line from A to B at T. */
	inline FVector Along(const FVector& A, const FVector& B, float T) { return A + (B - A) * T; }
	/** The Volcanic's hornito and wagon, shared with the kit. */
	void VolcanicHornito(FWRIsleSite& S, const FVector& Base, float H, float R);
	void VolcanicWagon(FWRIsleSite& S, const FVector& At, float Yaw, bool bTipped, int32 Salt);
	/** The Tundra's sledge, dead pine and tusk, shared with the kit. */
	void TundraSledge(FWRIsleSite& S, const FVector& At, float Yaw, int32 Salt, bool bLoad);
	void TundraDeadPine(FWRIsleSite& S, const FVector& G, float H, int32 Salt);
	void TundraTusk(FWRIsleSite& S, const FVector& Base, const FVector& Ctrl, const FVector& Tip, float Thick);
	/** World cm of a [x, y] metres array field of the site's row (false if missing). */
	bool RowPoint(const FWRIsleSite& S, const TCHAR* Obj, const TCHAR* Field, FVector2D& Out);
}
