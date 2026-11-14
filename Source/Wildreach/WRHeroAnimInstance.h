#pragma once

#include "CoreMinimal.h"
#include "WRAnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "WRHeroAnimInstance.generated.h"

class UPoseSearchDatabase;

/**
 * The player hero's animation (the MetaHuman male / female, Docs: cooks/hero.md 2.7 and 3.2, cooks/build-hero-anim.md).
 *
 * Built on UWRAnimInstance, which keeps everything above the legs exactly as before: the upper-body loops (block, bow
 * and rifle aim), the one-shots on the upper body or the whole body (swings, casts, hits, rolls, deaths, gathering),
 * turn in place and the feet planted on slopes. What changes is the bottom layer: locomotion is **motion matching**
 * over the GASP 5.8 clip library retargeted onto the hero (Content/Wildreach/Characters/Hero/Anims):
 *
 *   game thread  AWRCharacter fills HeroInput every tick. The trajectory is built here: the body's last 0.4 s as it
 *                really moved, and the next 1.0 s simulated the way the character movement will move it (the same
 *                acceleration, braking and turn rate, toward where the stick / the replicated acceleration points).
 *                The chooser (player_art.json hero.locomotion: state x gait x stance -> databases) picks what to
 *                search.
 *   worker       the proxy searches (every frame for your own hero, every anim.mm.remoteSearchInterval for others),
 *                jumps only when a better pose is found (the databases' own continuing-pose biases), and plays the
 *                result on a small blend stack at a rate that matches the real ground speed (stride matching).
 *                The clip's travel direction is warped onto the real one (orientation warping, the spine turning back
 *                so the chest keeps its facing), and the shown pose is fed back into the pose history the next search
 *                matches against.
 *   last         every bone but the root and pelvis takes the playing body's own bone lengths (one clip set fits the
 *                male, the female and every build), the pelvis scaled by the hips' height; the female idle stance
 *                (hero.md 2.3), the aim pitch over the spine, and foot plants for footsteps.
 *
 * Combat windows ride the clips as UWRANS_Window notify states (Data/anim_windows.json); one-shots forward them
 * through the proxy so they fire on every machine that draws the body.
 */

UENUM()
enum class EWRHeroGait : uint8 { Walk, Run, Sprint };

UENUM()
enum class EWRHeroMove : uint8 { Idle, Moving, Air, Land, Swim };

UENUM()
enum class EWRHeroStance : uint8 { Melee, Ranged, BowAim, RifleAim, Block };

/** One chooser row (player_art.json hero.locomotion): the first row that matches the hero's state wins. */
struct FWRHeroLocoRow
{
	EWRHeroMove State = EWRHeroMove::Idle;
	int32 Gait = -1;     // -1: any
	int32 Stance = -1;   // -1: any
	TArray<TObjectPtr<const UPoseSearchDatabase>> Databases;
};

/** Per-tick input from the owner (game thread). */
struct FWRHeroAnimInput
{
	FVector Velocity = FVector::ZeroVector;      // world, cm/s
	FVector Acceleration = FVector::ZeroVector;  // world, cm/s/s: where the stick (or the replicated acceleration) points
	float MaxSpeed = 450.f;                      // the speed the movement accelerates toward
	float MaxAcceleration = 5000.f;
	float BrakingDeceleration = 4000.f;
	float TurnRate = 540.f;                      // degrees/s the body turns toward its travel
	bool bOrientToMovement = true;               // false: the body keeps DesiredYaw (block, aim, first person, lock-on)
	float DesiredYaw = 0.f;                      // actor yaw it turns to when not oriented to movement
	EWRHeroGait Gait = EWRHeroGait::Run;
	EWRHeroMove Move = EWRHeroMove::Idle;
	EWRHeroStance Stance = EWRHeroStance::Melee;
	bool bLockedOn = false;
	bool bLocal = false;                         // your own hero: searches every frame
	float AimPitch = 0.f;                        // degrees, + up; spread over the spine while AimPitchAlpha > 0
	float AimPitchAlpha = 0.f;
};

/** What the proxy needs for the motion matching, handed over each frame. */
struct FWRHeroFrame
{
	FTransformTrajectory Trajectory;
	TArray<TObjectPtr<const UPoseSearchDatabase>> Databases;
	bool bActive = false;            // locomotion matches (not swimming, not dead)
	bool bInterrupt = false;         // the state changed (took off, landed): search now and take what's best
	float SearchInterval = 0.f;      // seconds between searches (0: every frame)
	float BlendTime = 0.2f;
	float Speed = 0.f;               // real ground speed, cm/s
	FVector MeshVelocity = FVector::ZeroVector;  // real velocity in the body mesh's space
	float MinRate = 0.8f, MaxRate = 1.25f;
	float WarpMaxDegrees = 45.f;
	float FemaleStance = 0.f;        // 0..1 (idle only)
	float PelvisScale = 1.f;         // the playing body's hip height over the clips' (1 when the body has the engine do it)
	bool bRetargetBones = false;     // bone lengths from the playing body (off when it's the body the clips were made on)
	float AimPitch = 0.f, AimPitchAlpha = 0.f;
	float FootContactHeight = 12.f;  // cm above the ground a foot counts as planted
	struct FShotNotify { const UAnimSequenceBase* Clip = nullptr; float From = 0.f, Delta = 0.f, Weight = 1.f; };
	TArray<FShotNotify> ShotNotifies;
};

struct FWRHeroAnimProxy : public FWRAnimProxy
{
	FWRHeroAnimProxy() = default;
	explicit FWRHeroAnimProxy(UAnimInstance* Instance) : FWRAnimProxy(Instance) {}

	FWRHeroFrame Hero;
	/** Read on the game thread: a foot touched down (bit 0 left, bit 1 right) since it was last cleared. */
	uint8 FootPlants = 0;
	/** The clip motion matching plays (for the log). */
	const UAnimSequenceBase* PlayingClip = nullptr;

protected:
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool EvaluateLoco(FPoseContext& Output) override;
	virtual void PostLayers(FPoseContext& Output) override;

private:
	struct FEntry
	{
		const UAnimSequenceBase* Clip = nullptr;
		const UPoseSearchDatabase* Database = nullptr;
		float Time = 0.f;
		float Blend = 0.f;    // 0..1 how far it has blended in
		float Weight = 0.f;   // final share this frame
		bool bLoop = false;
	};
	void Search(float DeltaSeconds);
	void StepStack(float DeltaSeconds);
	float RootSpeed(const FEntry& Entry, FVector* OutDir = nullptr) const;
	void TwistSpine(FCompactPose& Pose, float Degrees, const FVector& Axis) const;

	UE::PoseSearch::FPoseHistory History;
	bool bHistoryReady = false;
	bool bCacheBones = true;
	TArray<FBoneIndexType> HistoryBones;
	TArray<FEntry> Stack;
	float SinceSearch = 1000.f;
	bool bFootDown[2] = { true, true };
};

UCLASS(Transient, NotBlueprintable)
class WILDREACH_API UWRHeroAnimInstance : public UWRAnimInstance
{
	GENERATED_BODY()

public:
	/** The chooser rows (resolved databases) and the clips' source body (for the proportions). */
	void SetLocomotion(const TArray<FWRHeroLocoRow>& Rows, const TArray<FWRHeroLocoRow>& FallbackRows);
	/** The playing body is female: the idle stance. */
	void SetFemale(bool bInFemale) { bFemale = bInFemale; }
	/** Work out the proportion fix for the playing mesh from a clip's source body (its retarget pose). */
	void SetupProportions(const UAnimSequenceBase* AnyClip);

	FWRHeroAnimInput HeroInput;

	/** Footstep sound at each plant (WRSound::Footstep; your own always, others in earshot). */
	bool bHeroFootsteps = true;

	UPROPERTY(Transient) TArray<TObjectPtr<const UPoseSearchDatabase>> DatabaseRefs;  // keeps them loaded

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void PostStep(float DeltaSeconds, FWRAnimFrame& Frame) override;

private:
	void BuildTrajectory(float DeltaSeconds, FTransformTrajectory& Out);
	const FWRHeroLocoRow* Choose() const;

	TArray<FWRHeroLocoRow> LocoRows;
	TArray<FWRHeroLocoRow> FallbackRows;
	struct FPast { float Age = 0.f; FVector Position = FVector::ZeroVector; FQuat Facing = FQuat::Identity; };
	TArray<FPast> Past;           // newest first
	float SincePast = 0.f;
	EWRHeroMove LastMove = EWRHeroMove::Idle;
	double LandUntil = 0.0;
	bool bFemale = false;
	float StanceWeight = 0.f;
	float PelvisScale = 1.f;
	bool bRetargetBones = false;
	TMap<const UAnimSequenceBase*, float> ShotTimes;  // where each one-shot was last frame (for its notifies)
};
