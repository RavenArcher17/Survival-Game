#include "WRHeroAnimInstance.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchEvent.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchResult.h"

namespace WRHeroAnim
{
	/** The bones motion matching compares (the schemas' pose channels use the pelvis and feet; the rest are cheap
	 *  extras so a schema tuned later can use them without a code change). */
	static const TCHAR* HistoryBoneNames[] = { TEXT("root"), TEXT("pelvis"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r"),
		TEXT("thigh_l"), TEXT("thigh_r"), TEXT("hand_l"), TEXT("hand_r"), TEXT("spine_05"), TEXT("head") };

	static FCompactPoseBoneIndex Compact(const FBoneContainer& Bones, FName Name)
	{
		const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(Name);
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	}

	/** A bone's rotation in component space (the product of its parents' and its own). */
	static FQuat ParentRotation(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		FQuat Out = FQuat::Identity;
		for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Bone); P.GetInt() != INDEX_NONE; P = Bones.GetParentBoneIndex(P))
		{
			Out = Pose[P].GetRotation() * Out;
		}
		return Out;
	}

	/** Turn a bone by Turn (a rotation in component space) about its own joint; its children follow. */
	static void TurnBone(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& Turn)
	{
		if (Bone.GetInt() == INDEX_NONE)
		{
			return;
		}
		const FQuat Parent = ParentRotation(Pose, Bone);
		Pose[Bone].SetRotation((Parent.Inverse() * Turn * Parent * Pose[Bone].GetRotation()).GetNormalized());
	}

	static FVector ComponentLocation(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FTransform Out = FTransform::Identity;
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		for (FCompactPoseBoneIndex B = Bone; B.GetInt() != INDEX_NONE; B = Bones.GetParentBoneIndex(B))
		{
			Out = Out * Pose[B];
		}
		return Out.GetLocation();
	}

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
}

// ---------------------------------------------------------------- proxy (animation thread)

void FWRHeroAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FWRAnimProxy::Initialize(InAnimInstance);
	History.Initialize_AnyThread(10, 0.04f);
	bHistoryReady = false;
	bCacheBones = true;
	Stack.Reset();
	SinceSearch = 1000.f;
}

float FWRHeroAnimProxy::RootSpeed(const FEntry& Entry, FVector* OutDir) const
{
	if (!Entry.Clip)
	{
		return 0.f;
	}
	const float Length = Entry.Clip->GetPlayLength();
	const float Span = 0.1f;
	float From = Entry.Time, To = Entry.Time + Span;
	if (To > Length)
	{
		From = FMath::Max(0.f, Length - Span);
		To = Length;
	}
	if (To - From < 0.01f)
	{
		return 0.f;
	}
	const FTransform Motion = Entry.Clip->ExtractRootMotionFromRange(From, To, FAnimExtractContext());
	const FVector Move = Motion.GetTranslation() * FVector(1.f, 1.f, 0.f);
	if (OutDir)
	{
		*OutDir = Move.GetSafeNormal();
	}
	return float(Move.Size() / (To - From));
}

void FWRHeroAnimProxy::Search(float DeltaSeconds)
{
	if (!bHistoryReady || Hero.Databases.Num() == 0)
	{
		return;
	}
	using namespace UE::PoseSearch;
	const FEntry* Top = Stack.Num() > 0 ? &Stack.Last() : nullptr;
	const bool bNearEnd = Top && !Top->bLoop && Top->Clip && Top->Time > Top->Clip->GetPlayLength() - 0.15f;
	SinceSearch += DeltaSeconds;
	if (Top && !Hero.bInterrupt && !bNearEnd && SinceSearch < Hero.SearchInterval)
	{
		return;
	}
	SinceSearch = 0.f;

	const UObject* Context = GetAnimInstanceObject();
	const IPoseHistory* HistoryPtr = &History;
	const FRole Role = DefaultRole;
	TArray<const UObject*, TInlineAllocator<8>> Assets;
	for (const TObjectPtr<const UPoseSearchDatabase>& Database : Hero.Databases)
	{
		if (Database)
		{
			Assets.Add(Database.Get());
		}
	}
	FPoseSearchContinuingProperties Continuing;
	if (Top)
	{
		Continuing.PlayingAsset = Top->Clip;
		Continuing.PlayingAssetAccumulatedTime = Top->Time;
		Continuing.PlayingAssetDatabase = Top->Database;
	}
	Continuing.InterruptMode = Hero.bInterrupt ? EPoseSearchInterruptMode::InterruptOnDatabaseChange : EPoseSearchInterruptMode::DoNotInterrupt;
	const FSearchResult Result = UPoseSearchLibrary::MotionMatch(TArrayView<const UObject*>(&Context, 1), TArrayView<const FRole>(&Role, 1),
		TArrayView<const IPoseHistory*>(&HistoryPtr, 1), TArrayView<const UObject*>(Assets.GetData(), Assets.Num()), Continuing,
		FPoseSearchFutureProperties(), FPoseSearchEvent());
	if (!Result.IsValid() || Result.bIsContinuingPoseSearch || !Result.IsAssetTimeValid())
	{
		return;
	}
	const UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Result.GetCurrentResultAnimationAsset());
	if (!Clip)
	{
		return;
	}
	const float Time = Result.GetAssetTime();
	if (Top && Top->Clip == Clip && FMath::Abs(Top->Time - Time) < 0.2f)
	{
		return;  // the same moment of the same clip: keep playing it rather than restart a blend
	}
	FEntry& Entry = Stack.AddDefaulted_GetRef();
	Entry.Clip = Clip;
	Entry.Database = Result.Database.Get();
	Entry.Time = Time;
	Entry.Blend = Stack.Num() == 1 ? 1.f : 0.f;
	Entry.bLoop = Clip->bLoop;
	while (Stack.Num() > 4)
	{
		Stack.RemoveAt(0);
	}
	PlayingClip = Clip;
}

void FWRHeroAnimProxy::StepStack(float DeltaSeconds)
{
	if (Stack.Num() == 0)
	{
		return;
	}
	// Stride matching: the newest clip plays at the real ground speed over its own (within MinRate..MaxRate), and the
	// ones fading out keep that rate so their feet stay in step with it.
	float Rate = 1.f;
	const float ClipSpeed = RootSpeed(Stack.Last());
	if (ClipSpeed > 30.f && Hero.Speed > 30.f)
	{
		Rate = FMath::Clamp(Hero.Speed / ClipSpeed, Hero.MinRate, Hero.MaxRate);
	}
	const float BlendRate = Hero.BlendTime > KINDA_SMALL_NUMBER ? DeltaSeconds / Hero.BlendTime : 1.f;
	for (FEntry& Entry : Stack)
	{
		const float Length = FMath::Max(Entry.Clip->GetPlayLength(), 0.01f);
		Entry.Time += DeltaSeconds * Rate;
		Entry.Time = Entry.bLoop ? FMath::Fmod(Entry.Time, Length) : FMath::Min(Entry.Time, Length);
		Entry.Blend = FMath::Min(1.f, Entry.Blend + BlendRate);
	}
	// Shares, newest first: it takes its blend, the rest share what's left in the same way; the oldest takes the rest.
	float Remaining = 1.f;
	for (int32 i = Stack.Num() - 1; i >= 0; --i)
	{
		const float Share = i == 0 ? Remaining : Remaining * FMath::SmoothStep(0.f, 1.f, Stack[i].Blend);
		Stack[i].Weight = Share;
		Remaining -= Share;
	}
	while (Stack.Num() > 1 && Stack[0].Weight < 0.001f)
	{
		Stack.RemoveAt(0);
	}
}

void FWRHeroAnimProxy::Update(float DeltaSeconds)
{
	FWRAnimProxy::Update(DeltaSeconds);
	if (Hero.bActive)
	{
		History.SetTrajectory(Hero.Trajectory);
		Search(DeltaSeconds);
	}
	else
	{
		SinceSearch = 1000.f;  // search again as soon as it's back on its feet
	}
	StepStack(DeltaSeconds);

	// The one-shots' notifies (combat windows): what each crossed this frame.
	for (const FWRHeroFrame::FShotNotify& Shot : Hero.ShotNotifies)
	{
		if (Shot.Clip && Shot.Delta > 0.f)
		{
			FAnimNotifyContext Notifies;
			Shot.Clip->GetAnimNotifies(Shot.From, Shot.Delta, Notifies);
			if (Notifies.ActiveNotifies.Num() > 0)
			{
				AddAnimNotifies(Notifies.ActiveNotifies, Shot.Weight);
			}
		}
	}
}

bool FWRHeroAnimProxy::EvaluateLoco(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	bool bHave = false;
	if (Stack.Num() > 0)
	{
		FPoseContext Scratch(Output);
		bool bFirst = true;
		for (const FEntry& Entry : Stack)
		{
			if (!Entry.Clip || Entry.Weight <= ZERO_ANIMWEIGHT_THRESH)
			{
				continue;
			}
			FPoseContext& Dest = bFirst ? Output : Scratch;
			FAnimationPoseData Data(Dest);
			Entry.Clip->GetAnimationPose(Data, FAnimExtractContext(double(Entry.Time), false, FDeltaTimeRecord(), Entry.bLoop));
			const ScalarRegister Weight(Entry.Weight);
			if (bFirst)
			{
				bFirst = false;
				for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
				{
					Output.Pose[Bone] = Output.Pose[Bone] * Weight;
				}
			}
			else
			{
				for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
				{
					Output.Pose[Bone].AccumulateWithShortestRotation(Scratch.Pose[Bone], Weight);
				}
			}
		}
		if (!bFirst)
		{
			Output.Pose.NormalizeRotations();
			bHave = true;
		}
	}
	// The capsule carries the body: the root stays where the mesh is (its travel and turn are the clip's root motion).
	const FCompactPoseBoneIndex Root(0);
	if (bHave)
	{
		Output.Pose[Root] = Bones.GetRefPoseTransform(Root);
		if (Frame.TurnWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			// Turning in place (UWRAnimInstance steps the legs round with a turn clip, its own turn taken out): the
			// turn over the match.
			TArray<FWRAnimSample> Turn;
			for (const FWRAnimSample& Sample : Frame.Loco)
			{
				if (!Sample.bLoop)
				{
					Turn.Add(Sample);
					Turn.Last().Weight = 1.f;
				}
			}
			FPoseContext Over(Output);
			if (SampleBlend(Turn, Over))
			{
				const float Alpha = FMath::Min(Frame.TurnWeight, 1.f);
				for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
				{
					Output.Pose[Bone].BlendWith(Over.Pose[Bone], Alpha);
				}
			}
		}
	}
	else if (!SampleBlend(Frame.Loco, Output))
	{
		Output.ResetToRefPose();  // nothing matched yet and no idle to show
	}
	Output.Pose[Root].SetTranslation(Bones.GetRefPoseTransform(Root).GetTranslation());

	// Feed what's shown back into the pose history (the next search matches against it).
	if (HistoryBones.Num() == 0)
	{
		if (const USkeleton* SkeletonAsset = Bones.GetSkeletonAsset())
		{
			for (const TCHAR* Name : WRHeroAnim::HistoryBoneNames)
			{
				const int32 Index = SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(FName(Name));
				if (Index != INDEX_NONE)
				{
					HistoryBones.AddUnique(FBoneIndexType(Index));
				}
			}
		}
	}
	{
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		UE::PoseSearch::FComponentSpacePoseProvider Provider(CS);
		History.EvaluateComponentSpace_AnyThread(GetDeltaSeconds(), Provider, false, 0.f, 1.f, 1.f, !bHistoryReady, bCacheBones, HistoryBones);
		bCacheBones = false;
		bHistoryReady = true;
	}

	// Orientation warping: the clip walks one way, the body really goes another (strafes, a snappier turn than the
	// clip's): the whole pose turns onto the real travel and the spine turns back so the chest keeps the facing.
	if (Stack.Num() > 0 && Hero.Speed > 50.f && Hero.WarpMaxDegrees > 0.f)
	{
		FVector ClipDir;
		const float ClipSpeed = RootSpeed(Stack.Last(), &ClipDir);
		const FVector Real = Hero.MeshVelocity.GetSafeNormal2D();
		if (ClipSpeed > 50.f && !Real.IsNearlyZero() && !ClipDir.IsNearlyZero())
		{
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(ClipDir.X * Real.Y - ClipDir.Y * Real.X, ClipDir.X * Real.X + ClipDir.Y * Real.Y));
			const float Alpha = FMath::Clamp((Hero.Speed - 50.f) / 100.f, 0.f, 1.f);
			const float Warp = FMath::Clamp(Angle, -Hero.WarpMaxDegrees, Hero.WarpMaxDegrees) * Alpha;
			if (FMath::Abs(Warp) > 0.1f)
			{
				const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Warp));
				Output.Pose[Root].SetRotation((Turn * Output.Pose[Root].GetRotation()).GetNormalized());
				TwistSpine(Output.Pose, -Warp, FVector::UpVector);
			}
		}
	}
	return true;
}

void FWRHeroAnimProxy::TwistSpine(FCompactPose& Pose, float Degrees, const FVector& Axis) const
{
	static const FName Spine[] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const FQuat Step(Axis, FMath::DegreesToRadians(Degrees / UE_ARRAY_COUNT(Spine)));
	for (const FName& Name : Spine)
	{
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, Name), Step);
	}
}

void FWRHeroAnimProxy::PostLayers(FPoseContext& Output)
{
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Pelvis = WRHeroAnim::Compact(Bones, TEXT("pelvis"));

	// One clip set for every body: each bone keeps the playing body's own length (the engine's "Skeleton" translation
	// mode), the hips ride at this body's height ("AnimationScaled").
	if (Hero.bRetargetBones)
	{
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			if (Bone.GetInt() == 0 || Bone == Pelvis)
			{
				continue;
			}
			Pose[Bone].SetTranslation(Bones.GetRefPoseTransform(Bone).GetTranslation());
		}
	}
	if (Pelvis.GetInt() != INDEX_NONE && !FMath::IsNearlyEqual(Hero.PelvisScale, 1.f))
	{
		Pose[Pelvis].SetTranslation(Pose[Pelvis].GetTranslation() * Hero.PelvisScale);
	}

	const FVector Forward = Frame.MeshForward.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Frame.MeshForward.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);  // a positive turn about it tips the top forward

	// The female idle stance (hero.md 2.3): the pelvis tipped forward 3 degrees and dropped 2 toward the standing leg,
	// the back and legs turned back so only the hips move, the shoulders drawn back 2.
	if (Hero.FemaleStance > 0.01f && Pelvis.GetInt() != INDEX_NONE)
	{
		const float A = Hero.FemaleStance;
		const FQuat Tilt(Side, FMath::DegreesToRadians(3.f * A));
		const FQuat Roll(Forward, FMath::DegreesToRadians(2.f * A));
		const FQuat Hips = Roll * Tilt;
		WRHeroAnim::TurnBone(Pose, Pelvis, Hips);
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("spine_01")), Hips.Inverse());
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("thigh_l")), Hips.Inverse());
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("thigh_r")), Hips.Inverse());
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("clavicle_l")), FQuat(FVector::UpVector, FMath::DegreesToRadians(-2.f * A)));
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("clavicle_r")), FQuat(FVector::UpVector, FMath::DegreesToRadians(2.f * A)));
	}

	// Seated on a mount (cooks/cookpass-taming.md 3.6; a rider's seat): the hips flexed so the thighs reach forward over
	// the saddle, opened round the mount's back; the shins hanging back under the knees (heels under the hips); the feet
	// level in the stirrups; the torso leaning with the gait (upright at a walk, forward at a gallop); the hands carried
	// together low over the pommel on the reins, let go while a swing or a cast plays on the upper body.
	if (Hero.Seat > 0.01f)
	{
		const float W = Hero.Seat;
		for (int32 LegSide = 0; LegSide < 2; ++LegSide)
		{
			const float Out = LegSide == 0 ? -1.f : 1.f;   // left opens toward -Side, right toward +Side
			const FQuat Flex(Side, FMath::DegreesToRadians(-Hero.HipFlex * W));
			const FQuat Spread(Forward, FMath::DegreesToRadians(Out * Hero.SeatSpread * W));
			const FVector Hinge = Spread.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, LegSide == 0 ? TEXT("thigh_l") : TEXT("thigh_r")), Spread * Flex);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, LegSide == 0 ? TEXT("calf_l") : TEXT("calf_r")), FQuat(Hinge, FMath::DegreesToRadians(Hero.KneeBend * W)));
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, LegSide == 0 ? TEXT("foot_l") : TEXT("foot_r")), FQuat(Hinge, FMath::DegreesToRadians(Hero.FootLevel * W)));
		}
		TwistSpine(Pose, Hero.SeatLeanDeg * W, Side);
		const float A = W * Hero.SeatArms;
		if (A > 0.01f)
		{
			for (int32 ArmSide = 0; ArmSide < 2; ++ArmSide)
			{
				const float In = ArmSide == 0 ? 1.f : -1.f;   // the left arm comes in toward +Side, the right toward -Side
				const FQuat Lift(Side, FMath::DegreesToRadians(-Hero.ArmForward * A));
				const FQuat Inward(Forward, FMath::DegreesToRadians(In * Hero.ArmIn * A));
				const FVector Elbow = Inward.RotateVector(Side);
				WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, ArmSide == 0 ? TEXT("upperarm_l") : TEXT("upperarm_r")), Inward * Lift);
				WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, ArmSide == 0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r")), FQuat(Elbow, FMath::DegreesToRadians(-Hero.ElbowBend * A)));
			}
		}
	}

	// The lit torch carried in the off hand (cooks/cookpass-survival.md 3.7; BotW's torch carry): the left upper arm
	// lifted forward and a little out, the elbow bent so the flame rides at chest height ahead of the body, clear of the
	// thigh (the torch itself is held upright at the hand: AWRCharacter::TickSurvivalLook). Gives way to a one-shot on
	// the upper body (a swing, a cast) and to eating.
	{
		const float A = Hero.TorchCarry * Hero.ConsumeArms * (1.f - FMath::Clamp(Hero.Consume, 0.f, 1.f));
		if (A > 0.01f)
		{
			const FQuat Lift(Side, FMath::DegreesToRadians(-Hero.TorchArmForward * A));
			const FQuat Outward(Forward, FMath::DegreesToRadians(-Hero.TorchArmOut * A));
			const FVector Elbow = Outward.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("upperarm_l")), Outward * Lift);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("lowerarm_l")), FQuat(Elbow, FMath::DegreesToRadians(-Hero.TorchElbowBend * A)));
		}
	}

	// Eating or drinking (cooks/cookpass-survival.md 3.1; Monster Hunter's two bites, Elden Ring's committed use): the
	// left upper arm lifts forward and in across the chest and the elbow folds, so the hand and the food in it come to
	// the mouth; at each bite the hand pushes in and the head dips to meet it; a drink lifts the hand higher and tips the
	// head back for the pull. Eased in and out by the owner (AWRCharacter::FillConsumeInput); a one-shot on the upper body
	// (a real drink clip) takes over.
	if (Hero.Consume > 0.01f)
	{
		const float W = Hero.Consume * Hero.ConsumeArms;
		const float Bite = FMath::Clamp(Hero.ConsumeBite, 0.f, 1.f);
		if (W > 0.01f)
		{
			const float Fwd = Hero.EatArmForward + (Hero.bConsumeDrink ? Hero.EatDrinkLift : 0.f) + Bite * 6.f;
			const FQuat Lift(Side, FMath::DegreesToRadians(-Fwd * W));
			const FQuat Inward(Forward, FMath::DegreesToRadians(Hero.EatArmIn * W));   // the left arm comes in toward +Side (as the reins)
			const FVector Elbow = Inward.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("upperarm_l")), Inward * Lift);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("lowerarm_l")), FQuat(Elbow, FMath::DegreesToRadians(-(Hero.EatElbowBend + Bite * 8.f) * W)));
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("hand_l")), FQuat(Forward, FMath::DegreesToRadians(-20.f * W)));   // the wrist turns the food to the lips
		}
		// The head: dips into a bite, or tips back for a drink (over the neck and the head, 40 / 60).
		const float HeadDeg = Hero.bConsumeDrink ? -Hero.EatDrinkTilt * Hero.Consume : Hero.EatBiteDip * Bite * Hero.Consume;
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("neck_01")), FQuat(Side, FMath::DegreesToRadians(HeadDeg * 0.4f)));
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("head")), FQuat(Side, FMath::DegreesToRadians(HeadDeg * 0.6f)));
	}

	// Working with the hands (cooks/cookpass-crafting.md 3.1, 3.3): no clip in the hero set, so the arms are posed here
	// over the stance, on the beat's cycle. Knap: both forearms forward at the belly, the right hand bringing a hammerstone
	// down on the core in the left. Twist: the hands together at the belly rolling fibre against each other. Forge: the
	// left forearm forward and low on the tongs, the right arm raising the hand hammer high and bringing it down on the
	// anvil, the back leaning into the blow. Armor: the same, a shorter quicker blow. Plane: both hands on the plane, the
	// arms pushing it along the board and drawing it back, the body rocking with them. Pestle: the right hand grinding
	// round the mortar, the left steadying it. Gives way to a one-shot on the upper body.
	if (Hero.Work > 0.01f)
	{
		const FWRTuning& Tn = FWRTuning::Get();
		const float W = Hero.Work * Hero.ConsumeArms;
		const float P = FMath::Frac(Hero.WorkPhase);
		const auto Arm = [&](int32 S, float Fwd, float In, float ElbowDeg)
		{
			const float Sgn = S == 0 ? 1.f : -1.f;   // the left arm comes in toward +Side, the right toward -Side
			const FQuat Inward(Forward, FMath::DegreesToRadians(Sgn * In * W));
			const FQuat Lift(Side, FMath::DegreesToRadians(-Fwd * W));
			const FVector Hinge = Inward.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("upperarm_l") : TEXT("upperarm_r")), Inward * Lift);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r")), FQuat(Hinge, FMath::DegreesToRadians(-ElbowDeg * W)));
		};
		// A hammer's lift (0 down on the work .. 1 at the top): raised over most of the cycle, dropped fast onto the strike,
		// a small rebound off the work.
		const auto Hammer = [](float Phase, float Strike)
		{
			const float RaiseEnd = Strike * 0.82f;
			if (Phase < RaiseEnd) { const float T = Phase / RaiseEnd; return T * T * (3.f - 2.f * T); }
			if (Phase < Strike) { const float T = (Phase - RaiseEnd) / FMath::Max(0.01f, Strike - RaiseEnd); return 1.f - T * T; }
			return 0.1f * FMath::Sin(PI * (Phase - Strike) / FMath::Max(0.01f, 1.f - Strike));
		};
		float Lean = 0.f;
		switch (Hero.WorkKind)
		{
		case 1:   // knap
		{
			const float R = Hammer(P, Tn.Num(TEXT("craft.work.knap.strike"), 0.62f));
			Arm(0, Tn.Num(TEXT("craft.work.knap.holdForward"), 40.f), 22.f, 100.f);
			Arm(1, Tn.Num(TEXT("craft.work.knap.holdForward"), 40.f) + Tn.Num(TEXT("craft.work.knap.raise"), 24.f) * R, 16.f, 100.f + 16.f * R);
			Lean = Tn.Num(TEXT("craft.work.knap.lean"), 12.f);
			break;
		}
		case 2:   // twist
		{
			const float Roll = FMath::Sin(2.f * PI * P);
			Arm(0, 40.f + 5.f * Roll, 26.f, 106.f);
			Arm(1, 40.f - 5.f * Roll, 26.f, 106.f);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("hand_l")), FQuat(Forward, FMath::DegreesToRadians(24.f * Roll * W)));
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("hand_r")), FQuat(Forward, FMath::DegreesToRadians(-24.f * Roll * W)));
			Lean = 8.f;
			break;
		}
		case 3:   // forge
		case 4:   // armor
		{
			const bool bForge = Hero.WorkKind == 3;
			const TCHAR* K = bForge ? TEXT("forge") : TEXT("armor");
			const float R = Hammer(P, Tn.Num(*FString::Printf(TEXT("craft.work.%s.strike"), K), 0.62f));
			const float Raise = Tn.Num(*FString::Printf(TEXT("craft.work.%s.raise"), K), bForge ? 78.f : 48.f);
			Arm(0, Tn.Num(TEXT("craft.work.tongs.forward"), 42.f), 12.f, Tn.Num(TEXT("craft.work.tongs.elbow"), 55.f));
			Arm(1, 34.f + Raise * R, 8.f, 32.f + (bForge ? 80.f : 58.f) * R);
			Lean = Tn.Num(*FString::Printf(TEXT("craft.work.%s.lean"), K), 10.f) + 5.f * (1.f - R);
			break;
		}
		case 5:   // plane
		{
			const float Push = P < 0.5f ? FMath::SmoothStep(0.f, 1.f, P / 0.5f) : 1.f - FMath::SmoothStep(0.f, 1.f, (P - 0.5f) / 0.5f);
			Arm(0, 44.f + 24.f * Push, 12.f, 86.f - 48.f * Push);
			Arm(1, 44.f + 24.f * Push, 16.f, 86.f - 48.f * Push);
			Lean = Tn.Num(TEXT("craft.work.plane.lean"), 14.f) + 10.f * Push;
			break;
		}
		case 6:   // pestle
		{
			const float A = 2.f * PI * P;
			Arm(0, 44.f, 20.f, 92.f);
			Arm(1, 48.f + 6.f * FMath::Sin(A), 14.f + 8.f * FMath::Cos(A), 88.f);
			Lean = Tn.Num(TEXT("craft.work.pestle.lean"), 16.f);
			break;
		}
		default: break;
		}
		if (Lean != 0.f) TwistSpine(Pose, Lean * Hero.Work, Side);
	}

	// Asleep on the bed (cooks/cookpass-survival.md 3.7): the held lying pose breathes (the chest rises and falls about
	// 1.2 cm, a slow 0.22 Hz breath) and the head rests turned a little to one side, so a sleeper never reads as a body.
	if (Hero.Sleep > 0.01f)
	{
		const float Breath = FMath::Sin(Hero.Breath * 2.f * PI);
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("spine_04")), FQuat(Side, FMath::DegreesToRadians(-1.6f * Breath * Hero.Sleep)));
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("spine_05")), FQuat(Side, FMath::DegreesToRadians(1.0f * Breath * Hero.Sleep)));
		WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, TEXT("head")), FQuat(FVector::UpVector, FMath::DegreesToRadians(15.f * Hero.Sleep)));
	}

	// On a ladder (cookpass-building 3.5; no climb clip in the hero set, so the pose is built here): three points of
	// contact, one hand reaching up a rung while the opposite foot steps up; one full cycle every two rungs (the phase
	// comes from the body's height, so the hands keep time with the rungs and stop when the body stops). The chest leans
	// in to the rails.
	if (Hero.Climb > 0.01f)
	{
		const FWRTuning& Tn = FWRTuning::Get();
		const float W = Hero.Climb;
		const float ArmLow = Tn.Num(TEXT("anim.climb.armLow"), 95.f), ArmHigh = Tn.Num(TEXT("anim.climb.armHigh"), 150.f);
		const float ElbowLow = Tn.Num(TEXT("anim.climb.elbowLow"), 85.f), ElbowHigh = Tn.Num(TEXT("anim.climb.elbowHigh"), 25.f);
		const float ThighLow = Tn.Num(TEXT("anim.climb.thighLow"), 15.f), ThighHigh = Tn.Num(TEXT("anim.climb.thighHigh"), 72.f);
		const float KneeLow = Tn.Num(TEXT("anim.climb.kneeLow"), 20.f), KneeHigh = Tn.Num(TEXT("anim.climb.kneeHigh"), 100.f);
		for (int32 S = 0; S < 2; ++S)
		{
			// The left hand reaches while the right foot steps, and the other way round half a cycle later.
			const float Reach = 0.5f + 0.5f * FMath::Sin(Hero.ClimbPhase + (S == 0 ? 0.f : PI));
			const float Step = 1.f - Reach;
			const float In = S == 0 ? 1.f : -1.f;
			const FQuat Inward(Forward, FMath::DegreesToRadians(In * 12.f * W));
			const FQuat Lift(Side, FMath::DegreesToRadians(-FMath::Lerp(ArmLow, ArmHigh, Reach) * W));
			const FVector Elbow = Inward.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("upperarm_l") : TEXT("upperarm_r")), Inward * Lift);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r")), FQuat(Elbow, FMath::DegreesToRadians(-FMath::Lerp(ElbowLow, ElbowHigh, Reach) * W)));
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("thigh_l") : TEXT("thigh_r")), FQuat(Side, FMath::DegreesToRadians(-FMath::Lerp(ThighLow, ThighHigh, Step) * W)));
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("calf_l") : TEXT("calf_r")), FQuat(Side, FMath::DegreesToRadians(FMath::Lerp(KneeLow, KneeHigh, Step) * W)));
		}
		TwistSpine(Pose, Tn.Num(TEXT("anim.climb.lean"), 8.f) * W, Side);
	}

	// Working a siege engine's post (cookpass-building 3.5): leaning into a ram's handle with both arms out, turning a
	// winch's or windlass's crank (the hands go round with the handle, half a turn apart), or holding a ballista's grips.
	if (Hero.Crew > 0.01f)
	{
		const FWRTuning& Tn = FWRTuning::Get();
		const float W = Hero.Crew;
		const TCHAR* K = Hero.CrewKind == 0 ? TEXT("push") : Hero.CrewKind == 1 ? TEXT("crank") : TEXT("aim");
		const float Lean = Tn.Num(*FString::Printf(TEXT("anim.crew.%s.lean"), K), Hero.CrewKind == 0 ? 28.f : Hero.CrewKind == 1 ? 12.f : 6.f);
		const float Arm = Tn.Num(*FString::Printf(TEXT("anim.crew.%s.arm"), K), Hero.CrewKind == 0 ? 72.f : Hero.CrewKind == 1 ? 55.f : 50.f);
		const float ElbowDeg = Tn.Num(*FString::Printf(TEXT("anim.crew.%s.elbow"), K), Hero.CrewKind == 0 ? 32.f : Hero.CrewKind == 1 ? 60.f : 70.f);
		const float Swing = Hero.CrewKind == 1 ? Tn.Num(TEXT("anim.crew.crank.swing"), 18.f) : 0.f;
		const float Bend = Hero.CrewKind == 1 ? Tn.Num(TEXT("anim.crew.crank.bend"), 35.f) : 0.f;
		TwistSpine(Pose, Lean * W, Side);
		for (int32 S = 0; S < 2; ++S)
		{
			const float In = S == 0 ? 1.f : -1.f;
			const float Turn = Hero.CrewPhase + (S == 0 ? 0.f : PI);
			const FQuat Inward(Forward, FMath::DegreesToRadians(In * 10.f * W));
			// (Arms raised forward against the lean, so the hands stay at the handle's height.)
			const FQuat Lift(Side, FMath::DegreesToRadians(-(Arm + Swing * FMath::Sin(Turn)) * W));
			const FVector Hinge = Inward.RotateVector(Side);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("upperarm_l") : TEXT("upperarm_r")), Inward * Lift);
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, S == 0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r")), FQuat(Hinge, FMath::DegreesToRadians(-(ElbowDeg + Bend * (0.5f + 0.5f * FMath::Cos(Turn))) * W)));
		}
	}

	// Aim pitch over the upper spine: the chest, arms and weapon point where you look (first person, aiming).
	if (Hero.AimPitchAlpha > 0.01f)
	{
		static const FName Upper[] = { TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };
		const float Pitch = FMath::Clamp(Hero.AimPitch, -70.f, 70.f) * Hero.AimPitchAlpha;
		const FQuat Step(Side, FMath::DegreesToRadians(-Pitch / UE_ARRAY_COUNT(Upper)));
		for (const FName& Name : Upper)
		{
			WRHeroAnim::TurnBone(Pose, WRHeroAnim::Compact(Bones, Name), Step);
		}
	}

	// Foot plants (footsteps): a foot coming down to the ground while moving.
	static const FName Feet[2] = { TEXT("foot_l"), TEXT("foot_r") };
	for (int32 Foot = 0; Foot < 2; ++Foot)
	{
		const FCompactPoseBoneIndex Bone = WRHeroAnim::Compact(Bones, Feet[Foot]);
		if (Bone.GetInt() == INDEX_NONE)
		{
			continue;
		}
		const bool bDown = WRHeroAnim::ComponentLocation(Pose, Bone).Z < Hero.FootContactHeight;
		if (bDown && !bFootDown[Foot] && Hero.Speed > 50.f)
		{
			FootPlants |= uint8(1 << Foot);
		}
		bFootDown[Foot] = bDown;
	}
}

// ---------------------------------------------------------------- instance (game thread)

FAnimInstanceProxy* UWRHeroAnimInstance::CreateAnimInstanceProxy()
{
	return new FWRHeroAnimProxy(this);
}

void UWRHeroAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FWRHeroAnimProxy*>(InProxy);
}

void UWRHeroAnimInstance::SetLocomotion(const TArray<FWRHeroLocoRow>& Rows, const TArray<FWRHeroLocoRow>& Fallback)
{
	LocoRows = Rows;
	FallbackRows = Fallback;
	DatabaseRefs.Reset();
	for (const TArray<FWRHeroLocoRow>* List : { &LocoRows, &FallbackRows })
	{
		for (const FWRHeroLocoRow& Row : *List)
		{
			for (const TObjectPtr<const UPoseSearchDatabase>& Database : Row.Databases)
			{
				DatabaseRefs.AddUnique(Database);
			}
		}
	}
	Past.Reset();
}

void UWRHeroAnimInstance::SetupProportions(const UAnimSequenceBase* AnyClip)
{
	PelvisScale = 1.f;
	bRetargetBones = false;
	const UAnimSequence* Seq = Cast<UAnimSequence>(AnyClip);
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
	if (!Asset || !Skeleton)
	{
		return;
	}
	const TArray<FTransform>& Source = Seq->GetRetargetTransforms();  // the clips' own body (skeleton order)
	const FReferenceSkeleton& SkelRef = Skeleton->GetReferenceSkeleton();
	const FReferenceSkeleton& MeshRef = Asset->GetRefSkeleton();
	const auto Compare = [&](const TCHAR* Bone, float& OutRatio) -> bool
	{
		const int32 S = SkelRef.FindBoneIndex(Bone), M = MeshRef.FindBoneIndex(Bone);
		if (S == INDEX_NONE || M == INDEX_NONE || !Source.IsValidIndex(S))
		{
			return false;
		}
		const double From = Source[S].GetTranslation().Size(), To = MeshRef.GetRefBonePose()[M].GetTranslation().Size();
		OutRatio = From > 1.0 ? float(To / From) : 1.f;
		return true;
	};
	float Hips = 1.f, Thigh = 1.f;
	const bool bHips = Compare(TEXT("pelvis"), Hips);
	const bool bThigh = Compare(TEXT("calf_l"), Thigh);
	// The engine already does it if the skeleton asks for it: leave those bones alone.
	const int32 SkelPelvis = SkelRef.FindBoneIndex(TEXT("pelvis")), SkelCalf = SkelRef.FindBoneIndex(TEXT("calf_l"));
	const bool bEngineHips = SkelPelvis != INDEX_NONE && Skeleton->GetBoneTranslationRetargetingMode(SkelPelvis) == EBoneTranslationRetargetingMode::AnimationScaled;
	const bool bEngineBones = SkelCalf != INDEX_NONE && Skeleton->GetBoneTranslationRetargetingMode(SkelCalf) == EBoneTranslationRetargetingMode::Skeleton;
	PelvisScale = bHips && !bEngineHips ? Hips : 1.f;
	bRetargetBones = bThigh && !bEngineBones && (!FMath::IsNearlyEqual(Thigh, 1.f, 0.002f) || !FMath::IsNearlyEqual(Hips, 1.f, 0.002f));
	UE_LOG(LogWildreach, Log, TEXT("Hero anim: %s plays clips made on another body: hips x%.3f, bone lengths %s"), *Asset->GetName(), PelvisScale,
		bRetargetBones ? TEXT("from this body") : TEXT("as clipped"));
}

const FWRHeroLocoRow* UWRHeroAnimInstance::Choose() const
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const int32 FallbackLOD = int32(WRTune(TEXT("anim.mm.fallbackLOD"), 3.f));
	const bool bFar = Mesh && !HeroInput.bLocal && FallbackRows.Num() > 0 && Mesh->GetPredictedLODLevel() >= FallbackLOD;
	const TArray<FWRHeroLocoRow>& Rows = bFar ? FallbackRows : LocoRows;
	for (const FWRHeroLocoRow& Row : Rows)
	{
		if (Row.State == HeroInput.Move && (Row.Gait < 0 || Row.Gait == int32(HeroInput.Gait)) && (Row.Stance < 0 || Row.Stance == int32(HeroInput.Stance)))
		{
			return &Row;
		}
	}
	return nullptr;
}

void UWRHeroAnimInstance::BuildTrajectory(float DeltaSeconds, FTransformTrajectory& Out)
{
	Out.Samples.Reset();
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Owner = GetOwningActor();
	if (!Mesh || !Owner)
	{
		return;
	}
	const FTransform Comp = Mesh->GetComponentTransform();
	const float MeshYaw = FRotator::NormalizeAxis(float(Comp.Rotator().Yaw - Owner->GetActorRotation().Yaw));

	// The past, as the body really went (newest first in Past).
	const float PastStep = 1.f / 30.f, PastSpan = 0.45f;
	for (FPast& P : Past)
	{
		P.Age += DeltaSeconds;
	}
	SincePast += DeltaSeconds;
	if (Past.Num() == 0 || SincePast >= PastStep)
	{
		SincePast = 0.f;
		Past.Insert({ 0.f, Comp.GetLocation(), Comp.GetRotation() }, 0);
	}
	while (Past.Num() > 0 && Past.Last().Age > PastSpan)
	{
		Past.Pop();
	}
	for (int32 i = Past.Num() - 1; i >= 0; --i)
	{
		if (Past[i].Age > KINDA_SMALL_NUMBER)
		{
			FTransformTrajectorySample& S = Out.Samples.AddDefaulted_GetRef();
			S.TimeInSeconds = -Past[i].Age;
			S.Position = Past[i].Position;
			S.Facing = Past[i].Facing;
		}
	}
	{
		FTransformTrajectorySample& Now = Out.Samples.AddDefaulted_GetRef();
		Now.TimeInSeconds = 0.f;
		Now.Position = Comp.GetLocation();
		Now.Facing = Comp.GetRotation();
	}

	// The future, simulated the way the character movement moves: toward the stick's direction at full speed with its
	// acceleration, braking to a stop without input, the body turning toward its travel at its turn rate (or holding
	// the aimed facing).
	const FWRHeroAnimInput& In = HeroInput;
	FVector Velocity = In.Velocity * FVector(1.f, 1.f, 0.f);
	const FVector Accel = In.Acceleration * FVector(1.f, 1.f, 0.f);
	const bool bInput = Accel.SizeSquared() > 1.f;
	const FVector Desired = bInput ? Accel.GetSafeNormal() * In.MaxSpeed : FVector::ZeroVector;
	FVector Position = Comp.GetLocation();
	float Yaw = float(Comp.Rotator().Yaw);
	const float Step = 1.f / 30.f, Span = WRTune(TEXT("anim.mm.futureSeconds"), 1.0f);
	for (float T = Step; T <= Span + KINDA_SMALL_NUMBER; T += Step)
	{
		if (bInput)
		{
			const FVector Delta = Desired - Velocity;
			const float Max = In.MaxAcceleration * Step;
			Velocity += Delta.Size() > Max ? Delta.GetSafeNormal() * Max : Delta;
		}
		else
		{
			const float Speed = Velocity.Size();
			Velocity = Speed > In.BrakingDeceleration * Step ? Velocity * ((Speed - In.BrakingDeceleration * Step) / Speed) : FVector::ZeroVector;
		}
		Position += Velocity * Step;
		const float Goal = In.bOrientToMovement ? (Velocity.SizeSquared() > 100.f ? float(Velocity.Rotation().Yaw) + MeshYaw : Yaw)
		                                        : In.DesiredYaw + MeshYaw;
		Yaw = FMath::FixedTurn(Yaw, Goal, In.TurnRate * Step);
		FTransformTrajectorySample& S = Out.Samples.AddDefaulted_GetRef();
		S.TimeInSeconds = T;
		S.Position = Position;
		S.Facing = FRotator(0.f, Yaw, 0.f).Quaternion();
	}
}

void UWRHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// Where each one-shot was before this frame steps it (its notifies are what it crosses).
	ShotTimes.Reset();
	for (const FChannel* Channel : { &UpperShot, &FullShot })
	{
		for (const FTrack& Track : Channel->Tracks)
		{
			if (Track.Clip && Track.Target > 0.f)
			{
				ShotTimes.Add(Track.Clip, Track.Time);
			}
		}
	}
	Super::NativeUpdateAnimation(DeltaSeconds);
}

void UWRHeroAnimInstance::PostStep(float DeltaSeconds, FWRAnimFrame& Frame)
{
	FWRHeroAnimProxy& Proxy = GetProxyOnGameThread<FWRHeroAnimProxy>();
	FWRHeroFrame& Hero = Proxy.Hero;
	const FWRTuning& T = FWRTuning::Get();

	// Took off or landed: a short landing state picks the landing clips.
	EWRHeroMove Move = HeroInput.Move;
	if (LastMove == EWRHeroMove::Air && Move != EWRHeroMove::Air && Move != EWRHeroMove::Swim)
	{
		LandUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + T.Num(TEXT("anim.mm.landSeconds"), 0.35f) : 0.0;
	}
	if (GetWorld() && GetWorld()->GetTimeSeconds() < LandUntil && Move != EWRHeroMove::Air && Move != EWRHeroMove::Swim)
	{
		Move = EWRHeroMove::Land;
	}
	HeroInput.Move = Move;
	Hero.bInterrupt = Move != LastMove && (Move == EWRHeroMove::Air || Move == EWRHeroMove::Land || LastMove == EWRHeroMove::Air);
	LastMove = Move;

	const FWRHeroLocoRow* Row = Choose();
	Hero.Databases = Row ? Row->Databases : TArray<TObjectPtr<const UPoseSearchDatabase>>();
	Hero.bActive = Row != nullptr && Move != EWRHeroMove::Swim && Frame.FullLoopAlpha < 0.99f;
	BuildTrajectory(DeltaSeconds, Hero.Trajectory);
	Hero.SearchInterval = HeroInput.bLocal ? 0.f : T.Num(TEXT("anim.mm.remoteSearchInterval"), 0.1f);
	Hero.BlendTime = T.Num(TEXT("anim.mm.blendSeconds"), 0.2f);
	Hero.MinRate = T.Num(TEXT("anim.mm.minRate"), 0.8f);
	Hero.MaxRate = T.Num(TEXT("anim.mm.maxRate"), 1.25f);
	Hero.WarpMaxDegrees = T.Num(TEXT("anim.mm.warpMaxDegrees"), 45.f);
	Hero.FootContactHeight = T.Num(TEXT("anim.mm.footContactHeight"), 12.f);
	Hero.Speed = HeroInput.Velocity.Size2D();
	if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
	{
		Hero.MeshVelocity = Mesh->GetComponentTransform().InverseTransformVectorNoScale(HeroInput.Velocity);
	}
	Hero.PelvisScale = PelvisScale;
	Hero.bRetargetBones = bRetargetBones;
	Hero.AimPitch = HeroInput.AimPitch;
	Hero.AimPitchAlpha = HeroInput.AimPitchAlpha;
	// The rider's seat (cooks/cookpass-taming.md 3.6): numbers from tuning tame.seat; the hands let go of the reins while
	// a one-shot (a swing, a cast) has the upper body.
	Hero.Seat = HeroInput.Seat;
	Hero.SeatSpread = HeroInput.SeatSpread;
	Hero.SeatLeanDeg = FMath::Lerp(T.Num(TEXT("tame.seat.leanWalk"), 4.f), T.Num(TEXT("tame.seat.leanGallop"), 22.f), FMath::Clamp(HeroInput.SeatLean, 0.f, 1.f));
	Hero.SeatArms = 1.f - FMath::Clamp(FMath::Max(Frame.UpperShotAlpha, Frame.FullShotAlpha), 0.f, 1.f);
	Hero.HipFlex = T.Num(TEXT("tame.seat.hipFlex"), 72.f);
	Hero.KneeBend = T.Num(TEXT("tame.seat.kneeBend"), 76.f);
	Hero.FootLevel = T.Num(TEXT("tame.seat.footLevel"), -4.f);
	Hero.ArmForward = T.Num(TEXT("tame.seat.armForward"), 28.f);
	Hero.ArmIn = T.Num(TEXT("tame.seat.armIn"), 10.f);
	Hero.ElbowBend = T.Num(TEXT("tame.seat.elbowBend"), 64.f);
	Hero.Climb = HeroInput.Climb;
	Hero.ClimbPhase = HeroInput.ClimbPhase;
	Hero.Crew = HeroInput.Crew;
	Hero.CrewKind = HeroInput.CrewKind;
	Hero.CrewPhase = HeroInput.CrewPhase;
	// Eating and drinking, sleeping, the torch carry (cooks/cookpass-survival.md 3.1, 3.7; tuning food.pose, lights.carry).
	Hero.Work = HeroInput.Work;
	Hero.WorkKind = HeroInput.WorkKind;
	Hero.WorkPhase = HeroInput.WorkPhase;
	Hero.Consume = HeroInput.Consume;
	Hero.ConsumeBite = HeroInput.ConsumeBite;
	Hero.bConsumeDrink = HeroInput.bConsumeDrink;
	Hero.ConsumeArms = 1.f - FMath::Clamp(FMath::Max(Frame.UpperShotAlpha, HeroInput.Sleep > 0.01f ? 0.f : Frame.FullShotAlpha), 0.f, 1.f);
	Hero.EatArmForward = T.Num(TEXT("food.pose.armForward"), 58.f);
	Hero.EatArmIn = T.Num(TEXT("food.pose.armIn"), 26.f);
	Hero.EatElbowBend = T.Num(TEXT("food.pose.elbowBend"), 128.f);
	Hero.EatBiteDip = T.Num(TEXT("food.pose.biteDip"), 6.f);
	Hero.EatDrinkTilt = T.Num(TEXT("food.pose.drinkTilt"), 22.f);
	Hero.EatDrinkLift = T.Num(TEXT("food.pose.drinkLift"), 10.f);
	Hero.Sleep = HeroInput.Sleep;
	Hero.Breath = HeroInput.Breath;
	Hero.TorchCarry = HeroInput.TorchCarry;
	Hero.TorchArmForward = T.Num(TEXT("lights.carry.armForward"), 32.f);
	Hero.TorchElbowBend = T.Num(TEXT("lights.carry.elbowBend"), 70.f);
	Hero.TorchArmOut = T.Num(TEXT("lights.carry.armOut"), 8.f);

	// The female stance, only standing with nothing else on the body.
	const bool bStand = bFemale && HeroInput.Seat < 0.01f && Move == EWRHeroMove::Idle && Hero.Speed < 20.f && Frame.UpperShotAlpha < 0.01f && Frame.FullShotAlpha < 0.01f
		&& Frame.FullLoopAlpha < 0.01f && Frame.UpperLoopAlpha < 0.01f;
	StanceWeight = FMath::FInterpConstantTo(StanceWeight, bStand ? 1.f : 0.f, DeltaSeconds, 2.5f);
	Hero.FemaleStance = StanceWeight;

	// The one-shots' notifies this frame.
	Hero.ShotNotifies.Reset();
	for (const FChannel* Channel : { &UpperShot, &FullShot })
	{
		for (const FTrack& Track : Channel->Tracks)
		{
			if (!Track.Clip || Track.Target <= 0.f || Track.Weight <= 0.f)
			{
				continue;
			}
			const float* Before = ShotTimes.Find(Track.Clip);
			const float From = Before && *Before <= Track.Time ? *Before : 0.f;
			if (Track.Time > From)
			{
				Hero.ShotNotifies.Add({ Track.Clip, From, Track.Time - From, Track.Weight });
			}
		}
	}

	// Footsteps from the plants the last evaluation found.
	const uint8 Plants = Proxy.FootPlants;
	Proxy.FootPlants = 0;
	if (Plants && bHeroFootsteps && Move != EWRHeroMove::Air && Move != EWRHeroMove::Swim)
	{
		AActor* Owner = GetOwningActor();
		const APawn* Pawn = Cast<APawn>(Owner);
		const bool bMine = Pawn && Pawn->IsLocallyControlled();
		if (Owner && (bMine || WRHeroAnim::InEarshot(Owner, T.Num(TEXT("anim.footstepRange"), 3000.f))))
		{
			for (int32 Foot = 0; Foot < 2; ++Foot)
			{
				if (Plants & (1 << Foot))
				{
					WRSound::Footstep(Owner);
				}
			}
		}
	}
}
