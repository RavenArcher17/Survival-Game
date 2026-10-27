#include "WRAnimInstance.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ObjectKey.h"
#include "TwoBoneIK.h"
#include "HAL/IConsoleManager.h"

// Clip measurements: a locomotion clip's natural pace and foot plants, a turn clip's turn, an attack clip's strike.
// All from the clip's own bones (in place: the feet move back under the body at the ground speed while planted), once
// per clip, on the game thread.
namespace WRAnimDetail
{
	static TAutoConsoleVariable<int32> CVarFootSlide(TEXT("wr.AnimFootSlide"), 0,
		TEXT("Testing: log how fast planted feet slide over the ground (cm/s) every 2 s while moving"));

	/** Bones that meet the ground: toes, else feet / paws / hooves by name, else the lowest leaf bones. Left first. */
	static TArray<int32> FindFeet(const FReferenceSkeleton& Ref)
	{
		TArray<int32> Best;
		int32 BestScore = 0;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			const FString Name = Ref.GetBoneName(Bone).ToString().ToLower();
			if (Name.Contains(TEXT("end")) || Name.Contains(TEXT("tail")) || Name.StartsWith(TEXT("ik")))
			{
				continue;
			}
			const int32 Score = Name.Contains(TEXT("toebase")) ? 3 : Name.Contains(TEXT("toe")) ? 2
				: (Name.Contains(TEXT("foot")) || Name.Contains(TEXT("paw")) || Name.Contains(TEXT("hoof")) || Name.Contains(TEXT("ankle"))) ? 1 : 0;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best.Reset();
			}
			if (Score > 0 && Score == BestScore)
			{
				Best.Add(Bone);
			}
		}
		if (Best.Num() < 2)
		{
			// No names to go by: the leaf bones in the lowest sixth of the reference pose.
			Best.Reset();
			float MinZ = TNumericLimits<float>::Max(), MaxZ = -TNumericLimits<float>::Max();
			TArray<FTransform> CST;
			CST.SetNum(Ref.GetNum());
			for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
			{
				const int32 Parent = Ref.GetParentIndex(Bone);
				CST[Bone] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[Bone] : Ref.GetRefBonePose()[Bone] * CST[Parent];
				MinZ = FMath::Min(MinZ, float(CST[Bone].GetLocation().Z));
				MaxZ = FMath::Max(MaxZ, float(CST[Bone].GetLocation().Z));
			}
			TArray<bool> HasChild;
			HasChild.Init(false, Ref.GetNum());
			for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
			{
				if (Ref.GetParentIndex(Bone) != INDEX_NONE) HasChild[Ref.GetParentIndex(Bone)] = true;
			}
			for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
			{
				if (!HasChild[Bone] && CST[Bone].GetLocation().Z < MinZ + (MaxZ - MinZ) / 6.f && !Ref.GetBoneName(Bone).ToString().ToLower().Contains(TEXT("tail")))
				{
					Best.Add(Bone);
				}
			}
		}
		// Left foot first, so every clip of a skeleton lines up on the same foot.
		Best.StableSort([&Ref](int32 A, int32 B)
		{
			const bool bA = Ref.GetBoneName(A).ToString().Contains(TEXT("Left")), bB = Ref.GetBoneName(B).ToString().Contains(TEXT("Left"));
			return bA && !bB;
		});
		return Best;
	}

	/** A bone's component-space transform in a clip at a time (bones without a track keep the reference pose). */
	static FTransform BoneCS(const UAnimSequence* Seq, const FReferenceSkeleton& Ref, int32 Bone, double Time)
	{
		FTransform Out = FTransform::Identity;
		const FAnimExtractContext At(Time, false, FDeltaTimeRecord(), true);
		for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B))
		{
			FTransform Local = Ref.GetRefBonePose()[B];
			Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(B), At, false);
			Out = Out * Local;
		}
		return Out;
	}

	static FWRClipGait MeasureGait(const UAnimSequenceBase* Base)
	{
		static TMap<FObjectKey, FWRClipGait> Cache;
		if (const FWRClipGait* Hit = Cache.Find(FObjectKey(Base)))
		{
			return *Hit;
		}
		FWRClipGait Gait;
		const UAnimSequence* Seq = Cast<UAnimSequence>(Base);
		const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
		const double Length = Seq ? Seq->GetPlayLength() : 0.0;
		if (!Skeleton || Length <= KINDA_SMALL_NUMBER)
		{
			Cache.Add(FObjectKey(Base), Gait);
			return Gait;
		}
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		const TArray<int32> Feet = FindFeet(Ref);
		constexpr int32 N = 60;
		const double Step = Length / N;
		TArray<float> Speeds, Plants, Lifts;
		FVector2D Travel = FVector2D::ZeroVector;
		for (const int32 Foot : Feet)
		{
			TArray<FVector> P;
			P.SetNum(N);
			float MinZ = TNumericLimits<float>::Max(), MaxZ = -TNumericLimits<float>::Max();
			for (int32 i = 0; i < N; ++i)
			{
				P[i] = BoneCS(Seq, Ref, Foot, i * Step).GetLocation();
				MinZ = FMath::Min(MinZ, float(P[i].Z));
				MaxZ = FMath::Max(MaxZ, float(P[i].Z));
			}
			// How long the chain from the foot up to the root is: a foot must lift a little of that to count.
			float Reach = 1.f;
			for (int32 B = Foot; B != INDEX_NONE; B = Ref.GetParentIndex(B))
			{
				Reach += Ref.GetRefBonePose()[B].GetTranslation().Size();
			}
			const float Lift = MaxZ - MinZ;
			if (Lift < Reach * 0.02f)
			{
				continue;  // a foot that never lifts tells nothing (a tail tip, a still creature)
			}
			// Planted: within a tenth of its lift of its lowest (a looser line takes in the foot still coming down, which
			// moves with the body and makes a run look slower than it is).
			TArray<bool> Down;
			Down.SetNum(N);
			for (int32 i = 0; i < N; ++i)
			{
				Down[i] = P[i].Z < MinZ + FMath::Max(Lift * 0.1f, Reach * 0.01f);
			}
			TArray<float> Rates;
			int32 Plant = INDEX_NONE, LiftOff = INDEX_NONE;
			for (int32 i = 0; i < N; ++i)
			{
				const int32 Next = (i + 1) % N;
				if (Down[i] && Down[Next])
				{
					const FVector2D Move = FVector2D(P[Next] - P[i]) / Step;
					Rates.Add(Move.Size());
					Travel -= Move.GetSafeNormal();  // the planted foot slides back as the body goes forward
				}
				if (Down[i] && !Down[(i + N - 1) % N] && Plant == INDEX_NONE)
				{
					Plant = i;
				}
				if (Down[i] && !Down[Next] && LiftOff == INDEX_NONE)
				{
					LiftOff = Next;
				}
			}
			if (Rates.Num() >= 2 && Plant != INDEX_NONE && LiftOff != INDEX_NONE)
			{
				Rates.Sort();
				Speeds.Add(Rates[Rates.Num() / 2]);
				Plants.Add(float(Plant) / N);
				Lifts.Add(float(LiftOff) / N);
			}
		}
		Gait.Travel = Travel.GetSafeNormal();
		// Plants lined up on the first foot's: feet landing together (a trot's diagonal pair) make one step.
		const auto LineUp = [](const TArray<float>& Raw, float& OutFirst, TArray<float>& Out)
		{
			OutFirst = Raw[0];
			for (const float Phase : Raw)
			{
				const float Aligned = FMath::Fmod(Phase - OutFirst + 2.f, 1.f);
				if (!Out.ContainsByPredicate([Aligned](float Other) { const float D = FMath::Abs(Other - Aligned); return FMath::Min(D, 1.f - D) < 0.06f; }))
				{
					Out.Add(Aligned);
				}
			}
			Out.Sort();
		};
		if (Speeds.Num() > 0)
		{
			TArray<float> Sorted = Speeds;
			Sorted.Sort();
			Gait.Speed = Sorted[Sorted.Num() / 2];
			LineUp(Plants, Gait.FirstPlant, Gait.Plants);
			// Played backward, a foot touches down where it lifted off going forward.
			TArray<float> Backward;
			for (const float Phase : Lifts) Backward.Add(FMath::Fmod(2.f - Phase, 1.f));
			LineUp(Backward, Gait.FirstPlantBackward, Gait.PlantsBackward);
		}
		UE_LOG(LogWildreach, Log, TEXT("Anim clip %s: pace %.1f mesh units/s, %d of %d feet planting, first plant at %.2f"), *Base->GetName(),
			Gait.Speed, Speeds.Num(), Feet.Num(), Gait.FirstPlant);
		Cache.Add(FObjectKey(Base), Gait);
		return Gait;
	}

	/** Where in an attack clip (0..1) the head reaches furthest forward: the bite / swipe lands there. */
	static float MeasureStrike(const UAnimSequenceBase* Base, const FVector& Forward)
	{
		static TMap<FObjectKey, float> Cache;
		if (const float* Hit = Cache.Find(FObjectKey(Base)))
		{
			return *Hit;
		}
		float Strike = 0.5f;
		const UAnimSequence* Seq = Cast<UAnimSequence>(Base);
		const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
		if (Skeleton && Seq->GetPlayLength() > KINDA_SMALL_NUMBER)
		{
			const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
			int32 Head = INDEX_NONE;
			for (int32 Bone = 0; Bone < Ref.GetNum() && Head == INDEX_NONE; ++Bone)
			{
				const FString Name = Ref.GetBoneName(Bone).ToString().ToLower();
				if (Name == TEXT("head") || (Name.EndsWith(TEXT("head")) && !Name.Contains(TEXT("end"))))
				{
					Head = Bone;
				}
			}
			if (Head != INDEX_NONE)
			{
				const double Length = Seq->GetPlayLength();
				const FVector Start = BoneCS(Seq, Ref, Head, 0.0).GetLocation();
				float Best = 0.f;
				constexpr int32 N = 40;
				for (int32 i = 4; i <= N - 4; ++i)
				{
					const float Ahead = FVector::DotProduct(BoneCS(Seq, Ref, Head, Length * i / N).GetLocation() - Start, Forward);
					if (Ahead > Best)
					{
						Best = Ahead;
						Strike = float(i) / N;
					}
				}
			}
		}
		Cache.Add(FObjectKey(Base), Strike);
		return Strike;
	}

	/** Within earshot of a local player's view (footsteps of far creatures aren't worth playing). */
	static bool InEarshot(const AActor* Actor, float Range)
	{
		const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && PC->PlayerCameraManager
				&& FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Actor->GetActorLocation()) < FMath::Square(Range))
			{
				return true;
			}
		}
		return false;
	}

	/** Linear move toward a target at a rate (per second). */
	static float Approach(float Value, float Target, float Rate, float DeltaSeconds)
	{
		return Value < Target ? FMath::Min(Target, Value + Rate * DeltaSeconds) : FMath::Max(Target, Value - Rate * DeltaSeconds);
	}

	static float BlendRate(float Seconds)
	{
		return Seconds > KINDA_SMALL_NUMBER ? 1.f / Seconds : 1000.f;
	}
}

// ---------------------------------------------------------------- proxy (animation thread)

void FWRAnimProxy::RefreshMasks(const FBoneContainer& Bones)
{
	if (bMaskValid && MaskSerial == Bones.GetSerialNumber() && MaskRoot == Frame.UpperRoot)
	{
		return;
	}
	bMaskValid = true;
	MaskSerial = Bones.GetSerialNumber();
	MaskRoot = Frame.UpperRoot;
	const int32 Num = Bones.GetCompactPoseNumBones();
	UpperMask.Init(0, Num);
	const int32 RootMesh = Frame.UpperRoot.IsNone() ? INDEX_NONE : Bones.GetPoseBoneIndexForBoneName(Frame.UpperRoot);
	const FCompactPoseBoneIndex Root = RootMesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RootMesh));
	if (Root.GetInt() == INDEX_NONE)
	{
		UpperMask.Init(1, Num);  // no upper body named: upper layers cover everything
		return;
	}
	// Compact bones come parents first, so one pass marks the root's whole subtree.
	for (int32 i = 0; i < Num; ++i)
	{
		const FCompactPoseBoneIndex Bone(i);
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
		UpperMask[i] = (Bone == Root || (Parent.GetInt() != INDEX_NONE && UpperMask[Parent.GetInt()])) ? 1 : 0;
	}
}

bool FWRAnimProxy::SampleBlend(const TArray<FWRAnimSample>& Samples, FPoseContext& Out) const
{
	float Total = 0.f;
	int32 Used = 0;
	for (const FWRAnimSample& Sample : Samples)
	{
		if (Sample.Clip && Sample.Weight > ZERO_ANIMWEIGHT_THRESH)
		{
			Total += Sample.Weight;
			++Used;
		}
	}
	if (Used == 0)
	{
		return false;
	}
	FPoseContext Scratch(Out);
	bool bFirst = true;
	for (const FWRAnimSample& Sample : Samples)
	{
		if (!Sample.Clip || Sample.Weight <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}
		FPoseContext& Dest = bFirst ? Out : Scratch;
		FAnimationPoseData Data(Dest);
		Sample.Clip->GetAnimationPose(Data, FAnimExtractContext(double(Sample.Time), false, FDeltaTimeRecord(), Sample.bLoop));
		if (Sample.RemoveYaw != 0.f)
		{
			// A turn clip turns the body itself; the turn is shown by turning the whole pose instead (RootYaw), so take
			// the clip's own turn back out about the vertical through the feet.
			const FCompactPoseBoneIndex Root(0);
			Dest.Pose[Root] = Dest.Pose[Root] * FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(-Sample.RemoveYaw)));
		}
		const ScalarRegister Weight(Sample.Weight / Total);
		if (bFirst)
		{
			bFirst = false;
			if (Used > 1)
			{
				for (const FCompactPoseBoneIndex Bone : Out.Pose.ForEachBoneIndex())
				{
					Out.Pose[Bone] = Out.Pose[Bone] * Weight;
				}
			}
		}
		else
		{
			for (const FCompactPoseBoneIndex Bone : Out.Pose.ForEachBoneIndex())
			{
				Out.Pose[Bone].AccumulateWithShortestRotation(Scratch.Pose[Bone], Weight);
			}
		}
	}
	if (Used > 1)
	{
		for (const FCompactPoseBoneIndex Bone : Out.Pose.ForEachBoneIndex())
		{
			Out.Pose[Bone].NormalizeRotation();
		}
	}
	return true;
}

bool FWRAnimProxy::Evaluate(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	RefreshMasks(Bones);
	if (!SampleBlend(Frame.Loco, Output))
	{
		Output.ResetToRefPose();
	}

	// Upper layers: from the upper-body root up, and over the legs too while standing still.
	const auto Layer = [&](const TArray<FWRAnimSample>& Samples, float Alpha, bool bMasked)
	{
		if (Alpha <= ZERO_ANIMWEIGHT_THRESH)
		{
			return;
		}
		FPoseContext Over(Output);
		if (!SampleBlend(Samples, Over))
		{
			return;
		}
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			const float A = Alpha * (!bMasked || UpperMask[Bone.GetInt()] ? 1.f : 1.f - Frame.LegsMoving);
			if (A > ZERO_ANIMWEIGHT_THRESH)
			{
				Output.Pose[Bone].BlendWith(Over.Pose[Bone], FMath::Min(A, 1.f));
			}
		}
	};
	Layer(Frame.UpperLoop, Frame.UpperLoopAlpha, true);
	Layer(Frame.UpperShot, Frame.UpperShotAlpha, true);
	Layer(Frame.FullLoop, Frame.FullLoopAlpha, false);
	Layer(Frame.FullShot, Frame.FullShotAlpha, false);

	// Legs left behind by a swinging facing: the whole pose turns back about the vertical through the feet...
	if (FMath::Abs(Frame.RootYaw) > 0.01f)
	{
		const FCompactPoseBoneIndex Root(0);
		Output.Pose[Root] = Output.Pose[Root] * FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(Frame.RootYaw)));
	}
	// ...and the spine twists the upper body round to face where the actor faces.
	TArray<FCompactPoseBoneIndex, TInlineAllocator<4>> Spine;
	for (const FName& Name : Frame.SpineBones)
	{
		const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(Name);
		const FCompactPoseBoneIndex Bone = Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
		if (Bone.GetInt() != INDEX_NONE)
		{
			Spine.Add(Bone);
		}
	}
	if (Spine.Num() > 0 && FMath::Abs(Frame.SpineTwist) > 0.01f)
	{
		const FQuat Step(FVector::UpVector, FMath::DegreesToRadians(Frame.SpineTwist / Spine.Num()));
		for (const FCompactPoseBoneIndex Bone : Spine)
		{
			// The parent's rotation in component space, so the step turns about the component's vertical.
			FQuat Parent = FQuat::Identity;
			for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Bone); P.GetInt() != INDEX_NONE; P = Bones.GetParentBoneIndex(P))
			{
				Parent = Output.Pose[P].GetRotation() * Parent;
			}
			const FQuat Local = Output.Pose[Bone].GetRotation();
			Output.Pose[Bone].SetRotation((Parent.Inverse() * Step * Parent * Local).GetNormalized());
		}
	}
	PlantFeet(Output.Pose);
	return true;
}

void FWRAnimProxy::PlantFeet(FCompactPose& Pose) const
{
	// Standing on a slope: the hips sink to the lower foot's ground and the other leg bends to reach its own (two-bone
	// IK, the knee keeping its bend). FootLift is where the ground under each foot is against the capsule's bottom.
	if (Frame.LegBones.Num() != 6 || (FMath::Abs(Frame.FootLift[0]) < 0.5f && FMath::Abs(Frame.FootLift[1]) < 0.5f))
	{
		return;
	}
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	FCompactPoseBoneIndex Leg[6] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE),
	                                FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	for (int32 i = 0; i < 6; ++i)
	{
		const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(Frame.LegBones[i]);
		Leg[i] = Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
		if (Leg[i].GetInt() == INDEX_NONE)
		{
			return;
		}
	}
	const float Drop = FMath::Max(FMath::Min3(0.f, Frame.FootLift[0], Frame.FootLift[1]), -WRTune(TEXT("anim.footMaxDrop"), 35.f));
	const FCompactPoseBoneIndex Root(0);
	Pose[Root].AddToTranslation(FVector(0.f, 0.f, Drop));
	const auto CS = [&Pose, &Bones](FCompactPoseBoneIndex Bone)
	{
		FTransform Out = Pose[Bone];
		for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Bone); P.GetInt() != INDEX_NONE; P = Bones.GetParentBoneIndex(P))
		{
			Out = Out * Pose[P];
		}
		return Out;
	};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Raise = FMath::Min(Frame.FootLift[Side] - Drop, WRTune(TEXT("anim.footMaxRaise"), 45.f));
		if (Raise < 0.5f)
		{
			continue;
		}
		const FCompactPoseBoneIndex Thigh = Leg[Side * 3], Knee = Leg[Side * 3 + 1], Foot = Leg[Side * 3 + 2];
		const FTransform ThighParent = CS(Bones.GetParentBoneIndex(Thigh));
		FTransform A = Pose[Thigh] * ThighParent;
		FTransform B = Pose[Knee] * A;
		FTransform C = Pose[Foot] * B;
		const FVector Mid = (A.GetLocation() + C.GetLocation()) * 0.5f;
		const FVector Pole = B.GetLocation() + (B.GetLocation() - Mid).GetSafeNormal() * 60.f;
		AnimationCore::SolveTwoBoneIK(A, B, C, Pole, C.GetLocation() + FVector(0.f, 0.f, Raise), false, 1.0, 1.0);
		Pose[Thigh] = A.GetRelativeTransform(ThighParent);
		Pose[Knee] = B.GetRelativeTransform(A);
		Pose[Foot] = C.GetRelativeTransform(B);
		Pose[Thigh].NormalizeRotation();
		Pose[Knee].NormalizeRotation();
		Pose[Foot].NormalizeRotation();
	}
}

// ---------------------------------------------------------------- channels

void UWRAnimInstance::FChannel::Play(UAnimSequenceBase* Clip, float Rate, bool bLoop, bool bHold, float Start, float Delay, bool bInstant)
{
	for (FTrack& Track : Tracks)
	{
		Track.Target = 0.f;
	}
	FTrack& New = Tracks.AddDefaulted_GetRef();
	New.Clip = Clip;
	New.Length = FMath::Max(Clip->GetPlayLength(), 0.01f);
	New.Rate = Rate > KINDA_SMALL_NUMBER ? Rate : 1.f;
	New.Time = FMath::Clamp(Start, 0.f, New.Length);
	New.Delay = FMath::Max(0.f, Delay);
	New.bLoop = bLoop;
	New.bHold = bHold;
	New.Weight = bInstant ? 1.f : 0.f;
	New.Target = 1.f;
}

void UWRAnimInstance::FChannel::Loop(UAnimSequenceBase* Clip, float Start, bool bHold, float BlendIn)
{
	const FTrack* Top = Tracks.Num() > 0 && Tracks.Last().Target > 0.f ? &Tracks.Last() : nullptr;
	if (!Clip)
	{
		FadeAll();
	}
	else if (!Top || Top->Clip != Clip)
	{
		Play(Clip, 1.f, !bHold, bHold, Start, 0.f, false);
	}
}

void UWRAnimInstance::FChannel::FadeAll()
{
	for (FTrack& Track : Tracks)
	{
		Track.Target = 0.f;
	}
}

void UWRAnimInstance::FChannel::Step(float DeltaSeconds, float BlendIn, float BlendOut)
{
	for (int32 i = Tracks.Num() - 1; i >= 0; --i)
	{
		FTrack& Track = Tracks[i];
		if (Track.Delay > 0.f)
		{
			Track.Delay -= DeltaSeconds;
		}
		else
		{
			Track.Time += DeltaSeconds * Track.Rate;
			if (Track.bLoop)
			{
				Track.Time = FMath::Fmod(Track.Time, Track.Length);
			}
			else if (Track.Time >= Track.Length)
			{
				Track.Time = Track.Length;
				if (!Track.bHold)
				{
					Track.Target = 0.f;  // done: fade back to the layer below
				}
			}
		}
		Track.Weight = WRAnimDetail::Approach(Track.Weight, Track.Target, WRAnimDetail::BlendRate(Track.Target > Track.Weight ? BlendIn : BlendOut), DeltaSeconds);
		if (Track.Weight <= 0.f && Track.Target <= 0.f)
		{
			Tracks.RemoveAt(i);
		}
	}
}

float UWRAnimInstance::FChannel::Emit(TArray<FWRAnimSample>& Out) const
{
	Out.Reset();
	float Alpha = 0.f;
	for (const FTrack& Track : Tracks)
	{
		if (Track.Weight > 0.f)
		{
			FWRAnimSample& Sample = Out.AddDefaulted_GetRef();
			Sample.Clip = Track.Clip;
			Sample.Time = Track.Time;
			Sample.Weight = Track.Weight;
			Sample.bLoop = Track.bLoop;
			Alpha += Track.Weight;
		}
	}
	return FMath::Min(Alpha, 1.f);
}

bool UWRAnimInstance::FChannel::IsPlaying() const
{
	return Tracks.ContainsByPredicate([](const FTrack& Track) { return Track.Target > 0.f; });
}

float UWRAnimInstance::FTurnCurve::At(float Fraction) const
{
	if (Yaw.Num() == 0)
	{
		return 0.f;
	}
	const float F = FMath::Clamp(Fraction, 0.f, 1.f) * (Yaw.Num() - 1);
	const int32 I = FMath::Min(FMath::FloorToInt(F), Yaw.Num() - 2);
	return Yaw.Num() == 1 ? Yaw[0] : FMath::Lerp(Yaw[I], Yaw[I + 1], F - I);
}

// ---------------------------------------------------------------- instance (game thread)

FAnimInstanceProxy* UWRAnimInstance::CreateAnimInstanceProxy()
{
	return new FWRAnimProxy(this);
}

void UWRAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FWRAnimProxy*>(InProxy);
}

void UWRAnimInstance::ResetSetup()
{
	Sets.Reset();
	Clips.Reset();
	UpperLoop.Tracks.Reset();
	UpperShot.Tracks.Reset();
	FullLoop.Tracks.Reset();
	FullShot.Tracks.Reset();
	UpperRoot = NAME_None;
	SpineBones.Reset();
	LegBones.Reset();
	YawOffset = 0.f;
	bHaveYaw = false;
	bTurning = false;
	TurnWeight = 0.f;
}

float UWRAnimInstance::MeshScale() const
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	return Mesh ? float(Mesh->GetComponentScale().X) : 1.f;
}

UWRAnimInstance::FLocoSet* UWRAnimInstance::FindSet(FName Name)
{
	return Sets.FindByPredicate([Name](const FLocoSet& Set) { return Set.Name == Name; });
}

void UWRAnimInstance::AddLocoSet(FName Name, UAnimSequenceBase* Idle, const TArray<FWRLocoClipDesc>& Descs)
{
	FLocoSet& Set = Sets.AddDefaulted_GetRef();
	Set.Name = Name;
	Set.Idle = Idle;
	Set.Weight = Sets.Num() == 1 ? 1.f : 0.f;
	if (Idle)
	{
		Clips.AddUnique(Idle);
	}
	const float Scale = MeshScale();
	for (const FWRLocoClipDesc& Desc : Descs)
	{
		if (!Desc.Clip)
		{
			continue;
		}
		Clips.AddUnique(Desc.Clip);
		const FWRClipGait Gait = WRAnimDetail::MeasureGait(Desc.Clip);
		FLocoClip& Clip = Set.Clips.AddDefaulted_GetRef();
		Clip.Clip = Desc.Clip;
		Clip.Length = FMath::Max(Desc.Clip->GetPlayLength(), 0.05f);
		Clip.Angle = Desc.Angle;
		Clip.Gait = FMath::Clamp(Desc.Gait, 0, 1);
		Clip.Speed = Desc.Speed > 0.f ? Desc.Speed : Desc.Speed < 0.f ? 0.f : Gait.Speed * Scale;
		// The way its feet say it walks, against the way the data says. Opposite: the gait cycles its feet the wrong way
		// (the procedural quadruped rig moonwalks: planted feet slide forward), so it plays backward and walks right.
		// Off to the side: a clip filed under the wrong direction (worth a warning, once).
		float Off = 0.f;
		if (!Gait.Travel.IsNearlyZero() && GetSkelMeshComponent() && Desc.Speed >= 0.f)
		{
			const FVector Along = GetSkelMeshComponent()->GetRelativeRotation().RotateVector(FVector(Gait.Travel, 0.f));
			Off = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), Desc.Angle));
		}
		Clip.bBackward = Off > 135.f && Gait.PlantsBackward.Num() > 0;
		Clip.Offset = Clip.bBackward ? Gait.FirstPlantBackward : Gait.FirstPlant;
		const TArray<float>& Plants = Clip.bBackward ? Gait.PlantsBackward : Gait.Plants;
		Clip.Plants = Plants.Num() > 0 ? Plants : TArray<float>({ 0.f, 0.5f });
		static TSet<FObjectKey> Told;
		if (Off > 60.f && !Told.Contains(FObjectKey(Desc.Clip)))
		{
			Told.Add(FObjectKey(Desc.Clip));
			if (Clip.bBackward)
			{
				UE_LOG(LogWildreach, Log, TEXT("Anim %s: %s cycles its feet backward; played in reverse"), *Name.ToString(), *Desc.Clip->GetName());
			}
			else
			{
				UE_LOG(LogWildreach, Warning, TEXT("Anim %s: %s is set to walk at %.0f degrees but its feet go %.0f off that"), *Name.ToString(), *Desc.Clip->GetName(), Desc.Angle, Off);
			}
		}
		if (FMath::IsNearlyZero(Clip.Angle) && Clip.Speed > 0.f)
		{
			Set.GaitSpeed[Clip.Gait] = Clip.Speed;
			Set.bHasGait[Clip.Gait] = true;
		}
	}
	// A set with only one gait (or no forward clip measured) still needs its thresholds.
	for (int32 G = 0; G < 2; ++G)
	{
		if (!Set.bHasGait[G])
		{
			for (const FLocoClip& Clip : Set.Clips)
			{
				if (Clip.Gait == G)
				{
					Set.bHasGait[G] = true;
					Set.GaitSpeed[G] = FMath::Max(Set.GaitSpeed[G], Clip.Speed > 0.f ? Clip.Speed : (G == 0 ? 150.f : 400.f));
				}
			}
		}
	}
}

void UWRAnimInstance::SetTurnClips(FName SetName, UAnimSequenceBase* Left, UAnimSequenceBase* Right)
{
	if (FLocoSet* Set = FindSet(SetName))
	{
		// Pick by the turn each clip actually makes, whatever it's called.
		for (UAnimSequenceBase* Clip : { Left, Right })
		{
			if (!Clip)
			{
				continue;
			}
			Clips.AddUnique(Clip);
			const float Total = TurnCurve(Clip).Total;
			if (FMath::Abs(Total) > 20.f)
			{
				Set->Turn[Total > 0.f ? 1 : 0] = Clip;
			}
		}
	}
}

void UWRAnimInstance::SetUpperBody(FName Root, const TArray<FName>& Spine)
{
	UpperRoot = Root;
	SpineBones = Spine;
}

const UWRAnimInstance::FTurnCurve& UWRAnimInstance::TurnCurve(UAnimSequenceBase* Clip)
{
	if (const FTurnCurve* Found = TurnCurves.Find(Clip))
	{
		return *Found;
	}
	FTurnCurve& Curve = TurnCurves.Add(Clip);
	const UAnimSequence* Seq = Cast<UAnimSequence>(Clip);
	const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		return Curve;
	}
	// The root's turn about the vertical, relative to its first frame, unwrapped (a 180 turn passes the seam).
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	constexpr int32 N = 32;
	const FQuat First = WRAnimDetail::BoneCS(Seq, Ref, 0, 0.0).GetRotation();
	float Prev = 0.f, Unwrapped = 0.f;
	for (int32 i = 0; i <= N; ++i)
	{
		const FQuat Rel = WRAnimDetail::BoneCS(Seq, Ref, 0, Seq->GetPlayLength() * i / N).GetRotation() * First.Inverse();
		const FVector Ahead = Rel.RotateVector(FVector::ForwardVector);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
		Unwrapped += i == 0 ? 0.f : FMath::FindDeltaAngleDegrees(Prev, Yaw);
		Prev = Yaw;
		Curve.Yaw.Add(Unwrapped);
	}
	Curve.Total = Unwrapped;
	UE_LOG(LogWildreach, Log, TEXT("Anim turn clip %s turns %.0f degrees"), *Clip->GetName(), Curve.Total);
	return Curve;
}

float UWRAnimInstance::LocoSpeed(FName SetName, int32 Gait, float Angle) const
{
	const FLocoSet* Set = Sets.FindByPredicate([SetName](const FLocoSet& S) { return S.Name == SetName; });
	const FLocoClip* Clip = Set ? Set->Clips.FindByPredicate([Gait, Angle](const FLocoClip& C) { return C.Gait == Gait && FMath::IsNearlyEqual(C.Angle, Angle); }) : nullptr;
	return Clip ? Clip->Speed : 0.f;
}

float UWRAnimInstance::StrikeFraction(UAnimSequenceBase* Clip) const
{
	return Clip ? WRAnimDetail::MeasureStrike(Clip, MeshForward) : 0.5f;
}

float UWRAnimInstance::PlayOneShot(UAnimSequenceBase* Clip, EWRAnimLayer Layer, float Rate, float Start, float Delay, bool bHold)
{
	if (!Clip)
	{
		return 0.f;
	}
	Clips.AddUnique(Clip);
	Rate = Rate > KINDA_SMALL_NUMBER ? Rate : 1.f;
	FChannel& Channel = Layer == EWRAnimLayer::Full ? FullShot : UpperShot;
	Channel.Play(Clip, Rate, false, bHold, Start, Delay, false);
	if (Layer == EWRAnimLayer::Full)
	{
		UpperShot.FadeAll();  // a dodge or a death takes over from a swing
	}
	return Delay + FMath::Max(0.f, Clip->GetPlayLength() - Start) / Rate;
}

void UWRAnimInstance::StopOneShot(EWRAnimLayer Layer)
{
	(Layer == EWRAnimLayer::Full ? FullShot : UpperShot).FadeAll();
}

void UWRAnimInstance::StepLoco(float DeltaSeconds)
{
	const float Blend = WRTune(TEXT("anim.blendSeconds"), 0.2f);
	const float Speed = FMath::Max(Input.Speed, Input.TurnStepSpeed);
	const float Angle = FRotator::NormalizeAxis(Input.Angle);
	for (FLocoSet& Set : Sets)
	{
		Set.Weight = WRAnimDetail::Approach(Set.Weight, Set.Name == Input.Set ? 1.f : 0.f, WRAnimDetail::BlendRate(Blend), DeltaSeconds);
		// Gait: idle -> walk over the first half of the walk's pace, walk -> run between their paces.
		const float Walk = Set.bHasGait[0] ? Set.GaitSpeed[0] : Set.GaitSpeed[1];
		const float Run = Set.bHasGait[1] ? Set.GaitSpeed[1] : Set.GaitSpeed[0];
		const float StartMoving = FMath::Max(1.f, Walk * WRTune(TEXT("anim.idleToWalk"), 0.4f));
		const float Moving = FMath::Clamp((Speed - 5.f) / StartMoving, 0.f, 1.f);
		const float RunShare = !Set.bHasGait[0] ? 1.f : !Set.bHasGait[1] ? 0.f : FMath::Clamp((Speed - Walk) / FMath::Max(1.f, Run - Walk), 0.f, 1.f);
		// Direction: the two clips on either side of the travel direction, by angle (a diagonal is half of each).
		float Wants[2] = { 1.f - RunShare, RunShare };
		Set.IdleWeight = WRAnimDetail::Approach(Set.IdleWeight, Set.Idle ? 1.f - Moving : 0.f, WRAnimDetail::BlendRate(Blend), DeltaSeconds);
		for (int32 G = 0; G < 2; ++G)
		{
			int32 Lo = INDEX_NONE, Hi = INDEX_NONE;
			float LoGap = 1000.f, HiGap = 1000.f;
			for (int32 i = 0; i < Set.Clips.Num(); ++i)
			{
				if (Set.Clips[i].Gait != G)
				{
					continue;
				}
				const float Gap = FRotator::NormalizeAxis(Set.Clips[i].Angle - Angle);  // + : clip is clockwise of travel
				if (Gap >= 0.f && Gap < HiGap) { HiGap = Gap; Hi = i; }
				if (Gap <= 0.f && -Gap < LoGap) { LoGap = -Gap; Lo = i; }
			}
			if (Lo == INDEX_NONE) { Lo = Hi; LoGap = 0.f; }
			if (Hi == INDEX_NONE) { Hi = Lo; HiGap = 0.f; }
			for (int32 i = 0; i < Set.Clips.Num(); ++i)
			{
				FLocoClip& Clip = Set.Clips[i];
				if (Clip.Gait != G)
				{
					continue;
				}
				float Target = 0.f;
				if (Lo != INDEX_NONE && Hi != INDEX_NONE && (i == Lo || i == Hi))
				{
					const float Span = LoGap + HiGap;
					const float Share = Lo == Hi || Span < 0.01f ? 1.f : (i == Lo ? HiGap / Span : LoGap / Span);
					Target = Moving * Wants[G] * Share;
				}
				Clip.Weight = WRAnimDetail::Approach(Clip.Weight, Target, WRAnimDetail::BlendRate(Blend), DeltaSeconds);
			}
		}
	}

	// One foot phase for every clip: it runs at the ground speed over the blended stride (each clip's pace x its
	// length), so whichever clips show, the planted foot keeps up with the ground.
	float Weight = 0.f, Stride = 0.f, Natural = 0.f, Unknown = 0.f;
	const FLocoClip* Lead = nullptr;
	float LeadWeight = 0.f;
	for (const FLocoSet& Set : Sets)
	{
		for (const FLocoClip& Clip : Set.Clips)
		{
			const float W = Clip.Weight * Set.Weight;
			if (W <= 0.f)
			{
				continue;
			}
			Weight += W;
			Natural += W / Clip.Length;
			if (Clip.Speed > 0.f) Stride += W * Clip.Speed * Clip.Length;
			else Unknown += W;
			if (W > LeadWeight) { LeadWeight = W; Lead = &Clip; }
		}
	}
	const float OldPhase = Phase;
	if (Weight > KINDA_SMALL_NUMBER)
	{
		Natural /= Weight;
		// Clips with no known pace (a flyer's flap) just play at rate 1.
		const float Ratio = Stride > 0.f && Unknown < Weight * 0.5f
			? FMath::Clamp(Speed / (Stride / (Weight - Unknown)) / Natural, WRTune(TEXT("anim.minRate"), 0.5f), WRTune(TEXT("anim.maxRate"), 2.2f))
			: 1.f;
		Phase = FMath::Fmod(Phase + Natural * Ratio * DeltaSeconds, 1.f);
	}
	MovingWeight = Weight;
	IdleTime += DeltaSeconds * Input.IdleRate;
	if (Lead && bFootsteps)
	{
		Footsteps(OldPhase, Phase);
	}
}

void UWRAnimInstance::Footsteps(float OldPhase, float NewPhase) const
{
	if (!Input.bGrounded || MovingWeight < 0.5f || OldPhase == NewPhase || FullLoop.IsPlaying() || FullShot.IsPlaying())
	{
		return;
	}
	const FLocoClip* Lead = nullptr;
	float Best = 0.f;
	for (const FLocoSet& Set : Sets)
	{
		for (const FLocoClip& Clip : Set.Clips)
		{
			if (Clip.Weight * Set.Weight > Best) { Best = Clip.Weight * Set.Weight; Lead = &Clip; }
		}
	}
	if (!Lead)
	{
		return;
	}
	AActor* Owner = GetOwningActor();
	const APawn* Pawn = Cast<APawn>(Owner);
	const bool bMine = Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled();  // (AI pawns are "local" on a host)
	if (!Pawn || (!bMine && !WRAnimDetail::InEarshot(Owner, WRTune(TEXT("anim.footstepRange"), 3000.f))))
	{
		return;
	}
	for (const float Plant : Lead->Plants)
	{
		// Crossed this foot's touch-down (the phase may wrap past 1 this frame).
		const bool bCrossed = NewPhase >= OldPhase ? (Plant > OldPhase && Plant <= NewPhase) : (Plant > OldPhase || Plant <= NewPhase);
		if (bCrossed)
		{
			WRSound::Footstep(Owner);
		}
	}
}

void UWRAnimInstance::StepTurn(float DeltaSeconds, FLocoSet* Set)
{
	const float Blend = WRTune(TEXT("anim.blendSeconds"), 0.2f);
	const float Delta = bHaveYaw ? FMath::FindDeltaAngleDegrees(LastActorYaw, Input.ActorYaw) : 0.f;
	LastActorYaw = Input.ActorYaw;
	bHaveYaw = true;
	const bool bStanding = Input.bTurnInPlace && Input.Speed < 20.f && Set;
	if (!bStanding)
	{
		// Moving (or busy): the legs come round with the body.
		YawOffset = WRAnimDetail::Approach(YawOffset, 0.f, WRTune(TEXT("anim.turnCatchUp"), 540.f), DeltaSeconds);
		if (bTurning)
		{
			bTurning = false;
		}
	}
	else
	{
		YawOffset -= Delta;  // the facing swung; the planted legs stay where they were
		const float Limit = WRTune(TEXT("anim.turnMaxDegrees"), 120.f);
		YawOffset = FMath::Clamp(YawOffset, -Limit, Limit);
		if (bTurning)
		{
			// The turn clip steps the legs round: the pose turns as far as the clip has turned (scaled to the turn needed).
			const FTurnCurve& Curve = TurnCurve(TurnClip);
			const float Before = Curve.At(TurnTime / TurnLength);
			TurnTime = FMath::Min(TurnTime + DeltaSeconds, TurnLength);
			const float After = Curve.At(TurnTime / TurnLength);
			YawOffset += (After - Before) * TurnScale;
			if (TurnTime >= TurnLength)
			{
				bTurning = false;
			}
		}
		else if (FMath::Abs(YawOffset) > WRTune(TEXT("anim.turnThreshold"), 70.f))
		{
			// Legs this far behind: step round. The clip must turn the pose toward the facing, i.e. by -YawOffset.
			UAnimSequenceBase* Clip = Set->Turn[YawOffset < 0.f ? 1 : 0];
			const float Total = Clip ? TurnCurve(Clip).Total : 0.f;
			if (Clip && Total * -YawOffset > 0.f)
			{
				TurnClip = Clip;
				TurnTime = 0.f;
				TurnLength = FMath::Max(Clip->GetPlayLength(), 0.1f);
				TurnScale = FMath::Clamp(-YawOffset / Total, 0.5f, 1.5f);
				bTurning = true;
			}
			else
			{
				YawOffset = FMath::Clamp(YawOffset, -WRTune(TEXT("anim.turnThreshold"), 70.f), WRTune(TEXT("anim.turnThreshold"), 70.f));
			}
		}
	}
	TurnWeight = WRAnimDetail::Approach(TurnWeight, bTurning ? 1.f : 0.f, WRAnimDetail::BlendRate(Blend * 0.75f), DeltaSeconds);
	if (TurnWeight > 0.f && !bTurning && TurnTime < TurnLength)
	{
		TurnTime = FMath::Min(TurnTime + DeltaSeconds, TurnLength);  // fading out: let it finish its step
	}
}

void UWRAnimInstance::EmitLoco(TArray<FWRAnimSample>& Out) const
{
	Out.Reset();
	const float Rest = 1.f - TurnWeight;
	for (const FLocoSet& Set : Sets)
	{
		if (Set.Weight <= 0.f)
		{
			continue;
		}
		if (Set.Idle && Set.IdleWeight > 0.f)
		{
			FWRAnimSample& Sample = Out.AddDefaulted_GetRef();
			Sample.Clip = Set.Idle;
			Sample.Time = FMath::Fmod(IdleTime, FMath::Max(Set.Idle->GetPlayLength(), 0.01f));
			Sample.Weight = Set.IdleWeight * Set.Weight * Rest;
			Sample.bLoop = true;
		}
		for (const FLocoClip& Clip : Set.Clips)
		{
			if (Clip.Weight > 0.f)
			{
				FWRAnimSample& Sample = Out.AddDefaulted_GetRef();
				Sample.Clip = Clip.Clip;
				const float At = FMath::Fmod(Phase + Clip.Offset, 1.f);
				Sample.Time = (Clip.bBackward ? 1.f - At : At) * Clip.Length;
				Sample.Weight = Clip.Weight * Set.Weight * Rest;
				Sample.bLoop = true;
			}
		}
	}
	if (TurnWeight > 0.f && TurnClip)
	{
		FWRAnimSample& Sample = Out.AddDefaulted_GetRef();
		Sample.Clip = TurnClip;
		Sample.Time = TurnTime;
		Sample.Weight = TurnWeight;
		Sample.RemoveYaw = TurnCurves.Contains(TurnClip) ? TurnCurves[TurnClip].At(TurnTime / TurnLength) : 0.f;
	}
	if (Out.Num() == 0)
	{
		// Nothing chosen yet: the first set's idle (never the reference T-pose).
		for (const FLocoSet& Set : Sets)
		{
			if (Set.Idle)
			{
				FWRAnimSample& Sample = Out.AddDefaulted_GetRef();
				Sample.Clip = Set.Idle;
				Sample.Time = FMath::Fmod(IdleTime, FMath::Max(Set.Idle->GetPlayLength(), 0.01f));
				Sample.Weight = 1.f;
				Sample.bLoop = true;
				break;
			}
		}
	}
}

void UWRAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (Sets.Num() == 0)
	{
		return;
	}
	const float Blend = WRTune(TEXT("anim.blendSeconds"), 0.2f);
	FLocoSet* Set = FindSet(Input.Set);
	StepLoco(DeltaSeconds);
	StepTurn(DeltaSeconds, Set);
	// Standing still, the upper layers take the legs too (a swing steps, a block braces); moving, the legs walk on.
	LegsMoving = WRAnimDetail::Approach(LegsMoving, Input.Speed > WRTune(TEXT("anim.legsMovingSpeed"), 40.f) || bTurning ? 1.f : 0.f,
		WRAnimDetail::BlendRate(Blend), DeltaSeconds);

	UpperLoop.Loop(Input.UpperLoop, 0.f, false, Blend);
	UpperLoop.Step(DeltaSeconds, Blend, Blend);
	UpperShot.Step(DeltaSeconds, WRTune(TEXT("anim.shotBlendIn"), 0.1f), WRTune(TEXT("anim.shotBlendOut"), 0.2f));
	FullLoop.Loop(Input.FullLoop, Input.FullLoopStart, Input.bFullLoopHold, Blend);
	FullLoop.Step(DeltaSeconds, Blend, Blend);
	FullShot.Step(DeltaSeconds, WRTune(TEXT("anim.shotBlendIn"), 0.1f), WRTune(TEXT("anim.shotBlendOut"), 0.2f));

	FWRAnimFrame& Frame = GetProxyOnGameThread<FWRAnimProxy>().Frame;
	EmitLoco(Frame.Loco);
	Frame.UpperLoopAlpha = UpperLoop.Emit(Frame.UpperLoop);
	Frame.UpperShotAlpha = UpperShot.Emit(Frame.UpperShot);
	Frame.FullLoopAlpha = FullLoop.Emit(Frame.FullLoop);
	Frame.FullShotAlpha = FullShot.Emit(Frame.FullShot);
	Frame.LegsMoving = LegsMoving;
	Frame.RootYaw = YawOffset;
	// The upper body faces the actor: undo the legs' lag, and add any extra turn (a crossbow held across the body).
	Frame.SpineTwist = -YawOffset + Input.ExtraTwist;
	Frame.UpperRoot = UpperRoot;
	Frame.SpineBones = SpineBones;
	Frame.LegBones = LegBones;
	Frame.FootLift[0] = Input.FootLift[0];
	Frame.FootLift[1] = Input.FootLift[1];
	MeasureSlide(DeltaSeconds);
}

void UWRAnimInstance::MeasureSlide(float DeltaSeconds)
{
	// wr.AnimFootSlide 1 (testing): how fast the planted foot (the lowest) moves over the ground while the body walks.
	// Feet that keep up with the ground stay near 0; a pace that's off shows as a slide of that many cm/s.
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (WRAnimDetail::CVarFootSlide.GetValueOnGameThread() == 0 || !Mesh || !Mesh->GetSkeletalMeshAsset() || DeltaSeconds <= 0.f)
	{
		return;
	}
	if (SlideFeet.Num() == 0)
	{
		for (const int32 Bone : WRAnimDetail::FindFeet(Mesh->GetSkeletalMeshAsset()->GetRefSkeleton()))
		{
			SlideFeet.Add(Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(Bone));
		}
		SlidePrev.SetNum(SlideFeet.Num());
	}
	int32 Low = INDEX_NONE;
	TArray<FVector> Now;
	for (int32 i = 0; i < SlideFeet.Num(); ++i)
	{
		Now.Add(Mesh->GetBoneLocation(SlideFeet[i]));
		if (Low == INDEX_NONE || Now[i].Z < Now[Low].Z) Low = i;
	}
	if (Low != INDEX_NONE && Input.Speed > 50.f && Input.bGrounded && SlideHave)
	{
		SlideSamples.Add(FVector2D(Now[Low] - SlidePrev[Low]).Size() / DeltaSeconds);
		SlideSpeed += Input.Speed * DeltaSeconds;
		SlideTime += DeltaSeconds;
	}
	SlidePrev = Now;
	SlideHave = true;
	if (SlideTime >= 2.f)
	{
		SlideSamples.Sort();
		UE_LOG(LogWildreach, Log, TEXT("FOOTSLIDE %s set %s moving %.0f cm/s: planted foot slides %.0f cm/s (median of %d)"), *GetNameSafe(GetOwningActor()),
			*Input.Set.ToString(), SlideSpeed / SlideTime, SlideSamples[SlideSamples.Num() / 2], SlideSamples.Num());
		SlideSamples.Reset();
		SlideSpeed = SlideTime = 0.f;
	}
}
