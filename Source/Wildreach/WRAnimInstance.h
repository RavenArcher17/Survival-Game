#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "WRAnimInstance.generated.h"

class UAnimSequenceBase;

/**
 * Layered, blended animation for the player and the creatures, built in code (no animation blueprint), so it runs the
 * same headless, in the editor and in packaged builds.
 *
 * Layers, bottom to top:
 *   locomotion  idle / walk / run, blended by speed and by the direction of travel against the facing (forward, back,
 *               strafes, diagonals between them), all on one shared foot phase so the cycles stay in step; the phase
 *               runs at the real ground speed over each clip's stride, so the feet don't skate.
 *   upper       loops (block, rifle aim) and one-shots (swings, casts, hits, shots) from the upper-body root up; the
 *               legs keep the locomotion while moving and join in while standing.
 *   full        loops (fall, swim) and one-shots (dodge, death, gathering) over the whole body.
 * Every change crossfades (anim.* in tuning.json). Standing still, the legs stay put while the facing swings (the
 * upper body twists to follow) until a turn-in-place clip steps them round.
 *
 * Game thread: the owner fills Input every tick and calls PlayOneShot; NativeUpdateAnimation steps the state and hands
 * the proxy a list of weighted clip samples, which it blends on the animation thread.
 */

/** Where a one-shot or loop plays. */
enum class EWRAnimLayer : uint8
{
	Upper,
	Full,
};

/** One locomotion clip for AddLocoSet. */
struct FWRLocoClipDesc
{
	UAnimSequenceBase* Clip = nullptr;
	float Angle = 0.f;   // travel direction against the facing: 0 forward, 90 right, 180 back, -90 left
	int32 Gait = 0;      // 0 walk, 1 run
	float Speed = 0.f;   // cm/s at play rate 1; 0 = measure it from the clip's feet; < 0 = never speed-match (a wing flap)
};

/** What a clip's feet say about it (measured once per clip, see WRAnimDetail). */
struct FWRClipGait
{
	float Speed = 0.f;             // mesh units / s of ground speed at play rate 1 (0: couldn't tell)
	FVector2D Travel = FVector2D::ZeroVector;  // which way the clip walks, in mesh space
	float FirstPlant = 0.f;        // cycle phase (0..1) where the first foot touches down
	TArray<float> Plants;          // every foot's touch-down phase, lined up so FirstPlant is 0
	float FirstPlantBackward = 0.f;  // the same for the clip played backward (a gait whose feet cycle the wrong way)
	TArray<float> PlantsBackward;
};

/** Per-tick input from the owner. */
struct FWRAnimInput
{
	FName Set = NAME_None;             // locomotion set (AddLocoSet)
	float Speed = 0.f;                 // ground speed, cm/s
	float Angle = 0.f;                 // travel direction against the actor's facing, degrees
	float ActorYaw = 0.f;              // world yaw of the actor (turn in place keeps the legs while it swings)
	float TurnStepSpeed = 0.f;         // creatures turning on the spot: cm/s of stepping to show
	bool bTurnInPlace = false;         // standing and free to turn in place (not falling, swimming, mid one-shot)
	bool bGrounded = true;             // footsteps only on the ground
	float IdleRate = 1.f;              // e.g. slow breathing while asleep
	UAnimSequenceBase* UpperLoop = nullptr;  // e.g. block idle, rifle aim
	UAnimSequenceBase* FullLoop = nullptr;   // e.g. swimming, falling
	float FullLoopStart = 0.f;         // seconds into FullLoop to start from
	bool bFullLoopHold = false;        // play FullLoop once and hold its last frame (falling)
	float ExtraTwist = 0.f;            // degrees of upper-body turn on top of turn in place (a crossbow held across the body)
	float FootLift[2] = { 0.f, 0.f };  // cm the ground under the left / right foot sits above the capsule's bottom (slopes)
};

/** A clip sample for the animation thread. */
struct FWRAnimSample
{
	const UAnimSequenceBase* Clip = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	float RemoveYaw = 0.f;  // degrees taken off the pose's root about the vertical (a turn clip's own turn)
	bool bLoop = false;
};

/** Everything the animation thread needs for one frame. */
struct FWRAnimFrame
{
	TArray<FWRAnimSample> Loco, UpperLoop, UpperShot, FullLoop, FullShot;
	float UpperLoopAlpha = 0.f, UpperShotAlpha = 0.f, FullLoopAlpha = 0.f, FullShotAlpha = 0.f;
	float LegsMoving = 0.f;   // 0: the upper layer takes the whole body, 1: the legs keep the locomotion
	float RootYaw = 0.f;      // degrees: the whole pose about the vertical (legs left behind by a swinging facing)
	float SpineTwist = 0.f;   // degrees: spread over SpineBones so the upper body faces the actor
	FName UpperRoot;
	TArray<FName> SpineBones;
	float FootLift[2] = { 0.f, 0.f };
	TArray<FName> LegBones;   // left thigh, knee, foot, then right: feet planted on slopes (pelvis down, two-bone IK)
};

struct FWRAnimProxy : public FAnimInstanceProxy
{
	FWRAnimProxy() = default;
	explicit FWRAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	FWRAnimFrame Frame;

private:
	/** Sample and blend a weighted list into Out; false if the list is empty. */
	bool SampleBlend(const TArray<FWRAnimSample>& Samples, FPoseContext& Out) const;
	void RefreshMasks(const FBoneContainer& Bones);
	void PlantFeet(FCompactPose& Pose) const;
	TArray<uint8> UpperMask;  // per compact bone: 1 from the upper-body root up
	uint16 MaskSerial = 0;
	FName MaskRoot;
	bool bMaskValid = false;
};

UCLASS(Transient, NotBlueprintable)
class WILDREACH_API UWRAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Forget every set and clip (before a new body's art is set up). */
	void ResetSetup();
	/** A locomotion set: an idle and walk / run clips by direction. Call after the mesh has its scale (measured paces
	 *  are in mesh units). */
	void AddLocoSet(FName Name, UAnimSequenceBase* Idle, const TArray<FWRLocoClipDesc>& Clips);
	/** Turn-in-place clips for a set (they turn the body ~90 degrees each way; their own turn is measured). */
	void SetTurnClips(FName Set, UAnimSequenceBase* Left, UAnimSequenceBase* Right);
	/** Upper-body layer: the bone it starts at (and everything below it in the hierarchy), and the spine bones that
	 *  share the twist when the upper body turns away from the legs. */
	void SetUpperBody(FName Root, const TArray<FName>& Spine);
	/** Leg chains for planting the feet on slopes: left thigh, knee, foot, right thigh, knee, foot (empty: off). */
	void SetLegs(const TArray<FName>& Bones) { LegBones = Bones.Num() == 6 ? Bones : TArray<FName>(); }
	bool HasLegs() const { return LegBones.Num() == 6; }
	FName FootBone(int32 Side) const { return HasLegs() ? LegBones[Side == 0 ? 2 : 5] : NAME_None; }
	/** The body's forward in mesh space (for finding an attack clip's strike). */
	void SetMeshForward(const FVector& Forward) { MeshForward = Forward; }

	/** Play a clip once. Rate 0 keeps 1. Start skips into it, Delay holds its first frame first, bHold keeps the last
	 *  frame until the layer gets something else. Returns its play time in seconds (Delay included). */
	float PlayOneShot(UAnimSequenceBase* Clip, EWRAnimLayer Layer, float Rate = 1.f, float Start = 0.f, float Delay = 0.f, bool bHold = false);
	/** Fade out whatever one-shot plays on a layer. */
	void StopOneShot(EWRAnimLayer Layer);
	/** Measured ground speed of a set's clip at rate 1, cm/s (0 if unknown); for logs and tests. */
	float LocoSpeed(FName Set, int32 Gait, float Angle) const;
	/** Where the strike of an attack clip lands (0..1 of its length), from how far forward its head reaches. */
	float StrikeFraction(UAnimSequenceBase* Clip) const;

	FWRAnimInput Input;
	/** Calls WRSound::Footstep on the owner at each foot plant (players: always; creatures: within earshot). */
	bool bFootsteps = true;

	/** Keeps every clip in use loaded. */
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequenceBase>> Clips;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	struct FLocoClip
	{
		UAnimSequenceBase* Clip = nullptr;
		float Length = 1.f;
		float Speed = 0.f;      // cm/s at rate 1 (0: unknown, plays at rate 1)
		float Angle = 0.f;
		int32 Gait = 0;
		float Offset = 0.f;     // phase offset lining its first plant up with phase 0
		TArray<float> Plants;
		bool bBackward = false; // its feet cycle against its travel (a moonwalking gait): played backward
		float Weight = 0.f;     // smoothed
	};
	struct FLocoSet
	{
		FName Name;
		UAnimSequenceBase* Idle = nullptr;
		float IdleWeight = 1.f;
		TArray<FLocoClip> Clips;
		float GaitSpeed[2] = { 0.f, 0.f };
		bool bHasGait[2] = { false, false };
		float Weight = 0.f;
		UAnimSequenceBase* Turn[2] = { nullptr, nullptr };  // left, right
	};
	struct FTrack
	{
		UAnimSequenceBase* Clip = nullptr;
		float Time = 0.f, Rate = 1.f, Length = 1.f, Delay = 0.f;
		float Weight = 0.f, Target = 1.f;
		bool bLoop = false, bHold = false;
	};
	struct FChannel
	{
		TArray<FTrack> Tracks;
		void Play(UAnimSequenceBase* Clip, float Rate, bool bLoop, bool bHold, float Start, float Delay, bool bInstant);
		void Loop(UAnimSequenceBase* Clip, float Start, bool bHold, float BlendIn);
		void FadeAll();
		void Step(float DeltaSeconds, float BlendIn, float BlendOut);
		float Emit(TArray<FWRAnimSample>& Out) const;  // returns the layer's alpha
		bool IsPlaying() const;
	};
	struct FTurnCurve
	{
		TArray<float> Yaw;  // degrees turned, sampled evenly over the clip (unwrapped)
		float Total = 0.f;
		float At(float Fraction) const;
	};

	void StepLoco(float DeltaSeconds);
	void StepTurn(float DeltaSeconds, FLocoSet* Set);
	void EmitLoco(TArray<FWRAnimSample>& Out) const;
	void Footsteps(float OldPhase, float NewPhase) const;
	const FTurnCurve& TurnCurve(UAnimSequenceBase* Clip);
	float MeshScale() const;
	void MeasureSlide(float DeltaSeconds);  // wr.AnimFootSlide (testing)
	TArray<FName> SlideFeet;
	TArray<FVector> SlidePrev;
	TArray<float> SlideSamples;
	float SlideSpeed = 0.f, SlideTime = 0.f;
	bool SlideHave = false;
	FLocoSet* FindSet(FName Name);

	TArray<FLocoSet> Sets;
	FChannel UpperLoop, UpperShot, FullLoop, FullShot;
	float Phase = 0.f;
	float IdleTime = 0.f;
	float LegsMoving = 0.f;
	float MovingWeight = 0.f;  // locomotion's non-idle share (footsteps only while really walking)
	FName UpperRoot;
	TArray<FName> SpineBones;
	TArray<FName> LegBones;
	FVector MeshForward = FVector::ForwardVector;

	// Turn in place
	float YawOffset = 0.f;      // degrees the legs are behind the actor's facing
	float LastActorYaw = 0.f;
	bool bHaveYaw = false;
	UAnimSequenceBase* TurnClip = nullptr;
	float TurnTime = 0.f, TurnLength = 1.f, TurnScale = 1.f, TurnWeight = 0.f;
	bool bTurning = false;
	TMap<UAnimSequenceBase*, FTurnCurve> TurnCurves;
};
