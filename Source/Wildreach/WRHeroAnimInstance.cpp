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

	// The female stance, only standing with nothing else on the body.
	const bool bStand = bFemale && Move == EWRHeroMove::Idle && Hero.Speed < 20.f && Frame.UpperShotAlpha < 0.01f && Frame.FullShotAlpha < 0.01f
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
