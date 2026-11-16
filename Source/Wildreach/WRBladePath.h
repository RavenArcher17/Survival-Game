#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

/**
 * Where a survivor's drawn weapon really is through a swing's strike (cooks/cookpass-combat.md 3.1): the hero's own
 * swing clip, sampled once per clip on whichever machine judges the blow (a dedicated server too, which never loads the
 * body to draw it). For each clip, the fist's frame (the weapon socket turned as WRCharacter's FistFrame: X out of the
 * fist past the index finger, Y toward the fingers) in the body mesh's component space at 16 points across the clip's
 * strike frames (anim_windows.json "trace"), the same frames the clip warp fits onto the move's active window.
 *
 * The swing (UWRGA_Melee::Sweep) puts the class's blade row (weapons.json blade) in that frame, so what it hits is what
 * the drawn edge passed through, when it passed.
 */
class WILDREACH_API FWRBladePaths
{
public:
	static FWRBladePaths& Get();

	/** The fist's frame in the ACTOR's space at strike fraction F (0 = the first strike frame, 1 = the last) of the hero
	 *  body Sex ("m" / "f"), clip set Key ("light" / "heavy"), clip Variant (the move row's clip, wrapped like the visual).
	 *  HalfHeight: the swinger's capsule half height (the body sits on the capsule's foot). False: no path for that clip
	 *  (no such clip, not an animation sequence, no socket, or a frame that put the blade behind the body); the swing then
	 *  keeps the reach tube. */
	bool FistAt(FName Sex, FName Key, int32 Variant, float F, float HalfHeight, FTransform& Out);

	/** The fist's frame in a body mesh's component space from its reference pose (the weapon socket, turned so Y points
	 *  along the fingers). Shared with AWRCharacter::FistFrame (how the weapon is held). */
	static FTransform FistFrameOf(const USkeletalMesh* Mesh, FName Socket);

private:
	struct FPath
	{
		bool bValid = false;
		/** Component space, one per sample across the strike frames. */
		TArray<FTransform> Fist;
		/** The body's yaw on the capsule and the mesh's lowest point (its base height = -half height - min(MinZ, 0)). */
		float Yaw = -90.f;
		float MinZ = 0.f;
	};
	bool Build(FName Sex, FName Key, int32 Variant, FPath& Out) const;
	TMap<FString, FPath> Paths;
};
