#include "WRBladePath.h"
#include "Wildreach.h"
#include "WRAnimNotifies.h"
#include "WRTuning.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "BoneIndices.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Dom/JsonObject.h"

namespace WRBladePathDetail
{
	constexpr int32 Samples = 16;

	FString Field(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Key, V);
		return V;
	}
}

FWRBladePaths& FWRBladePaths::Get()
{
	static FWRBladePaths Instance;
	return Instance;
}

FTransform FWRBladePaths::FistFrameOf(const USkeletalMesh* Mesh, FName SocketName)
{
	// The weapon socket's own frame: X out of the fist (index side); Y turned toward the fingers (the knuckles' way).
	const USkeletalMeshSocket* Socket = Mesh ? Mesh->FindSocket(SocketName) : nullptr;
	if (!Socket)
	{
		return FTransform::Identity;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(Socket->BoneName);
	const FString Side = Socket->BoneName.ToString().Right(2);  // "_r" / "_l"
	const int32 Finger = Ref.FindBoneIndex(FName(*(TEXT("middle_01") + Side)));
	if (Bone == INDEX_NONE || Finger == INDEX_NONE)
	{
		return FTransform::Identity;
	}
	const FTransform SocketCS = Socket->GetSocketLocalTransform() * FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone);
	const FVector Fingers = SocketCS.InverseTransformVectorNoScale(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Finger).GetLocation()
		- FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation());
	FVector Y = FVector::VectorPlaneProject(Fingers, FVector::XAxisVector).GetSafeNormal();
	if (Y.IsNearlyZero())
	{
		Y = FVector::YAxisVector;
	}
	return FTransform(FQuat(FRotationMatrix::MakeFromXY(FVector::XAxisVector, Y)), FVector::ZeroVector);
}

bool FWRBladePaths::Build(FName Sex, FName Key, int32 Variant, FPath& Out) const
{
	using namespace WRBladePathDetail;
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject>* Heroes = nullptr;
	const TSharedPtr<FJsonObject>* HeroPtr = nullptr;
	if (!Art.IsValid() || !Art->TryGetObjectField(TEXT("hero"), Heroes) || !(*Heroes)->TryGetObjectField(Sex == TEXT("f") ? TEXT("f") : TEXT("m"), HeroPtr))
	{
		return false;
	}
	// The body: the default face's (faces[0]) or the hero entry's own, as AWRCharacter::LoadBodyArt picks it.
	FString BodyPath;
	const TArray<TSharedPtr<FJsonValue>>* Faces = nullptr;
	if ((*HeroPtr)->TryGetArrayField(TEXT("faces"), Faces) && Faces->Num() > 0)
	{
		BodyPath = Field((*Faces)[0]->AsObject(), TEXT("body"));
	}
	if (BodyPath.IsEmpty())
	{
		BodyPath = Field(*HeroPtr, TEXT("body"));
	}
	const USkeletalMesh* Mesh = BodyPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *BodyPath);
	FString SocketName = TEXT("weapon_r");
	const TSharedPtr<FJsonObject>* Sockets = nullptr;
	if (Art->TryGetObjectField(TEXT("sockets"), Sockets)) (*Sockets)->TryGetStringField(TEXT("weapon"), SocketName);
	const USkeletalMeshSocket* Socket = Mesh ? Mesh->FindSocket(FName(*SocketName)) : nullptr;

	// The clip: the same set and wrapped variant the body plays (WRCharacter::OnSwingVisual).
	const TSharedPtr<FJsonObject>* Anims = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Set = nullptr;
	if (!Socket || !Art->TryGetObjectField(TEXT("anims"), Anims) || !(*Anims)->TryGetArrayField(Key.ToString(), Set) || Set->Num() == 0)
	{
		return false;
	}
	const FString Name = (*Set)[FMath::Abs(Variant) % Set->Num()]->AsString();
	const FString ClipPath = Name.StartsWith(TEXT("/")) ? Name : Field(Art, TEXT("animDir")) + Name;
	UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s.%s"), *ClipPath, *FPaths::GetBaseFilename(ClipPath)));
	USkeleton* Skeleton = Clip ? Clip->GetSkeleton() : nullptr;
	if (!Clip || !Skeleton)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Blade path: %s is not an animation sequence; %s swings use the reach tube"), *ClipPath, *Key.ToString());
		return false;
	}

	// The bone chain from the root to the socket's bone.
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	TArray<int32> Chain;
	for (int32 B = Ref.FindBoneIndex(Socket->BoneName); B != INDEX_NONE; B = Ref.GetParentIndex(B))
	{
		Chain.Insert(B, 0);
	}
	if (Chain.Num() == 0)
	{
		return false;
	}
	TArray<int32> SkeletonIndex;
	for (const int32 B : Chain)
	{
		SkeletonIndex.Add(Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(Mesh, B));
	}

	float S = 0.4f, E = 0.55f;
	FWRAnimWindows::Find(Clip->GetFName(), TEXT("trace"), S, E);
	const float Length = Clip->GetPlayLength();
	const float T0 = S * Length, T1 = FMath::Max(E * Length, T0 + 0.02f);
	const FTransform Fist = FistFrameOf(Mesh, Socket->SocketName);
	const FTransform SocketLocal = Socket->GetSocketLocalTransform();

	Out.Fist.Reset();
	for (int32 k = 0; k < Samples; ++k)
	{
		const FAnimExtractContext Context(double(FMath::Lerp(T0, T1, float(k) / float(Samples - 1))));
		FTransform CS = FTransform::Identity;
		for (int32 i = 0; i < Chain.Num(); ++i)
		{
			FTransform Local = Ref.GetRefBonePose()[Chain[i]];
			if (SkeletonIndex[i] != INDEX_NONE)
			{
				Clip->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex[i]), Context, false);
			}
			if (i == 0)
			{
				// The root's travel is the ability's to make (step-in, lunges move the capsule): keep the blade on the body.
				Local.SetTranslation(FVector(0.f, 0.f, Local.GetTranslation().Z));
			}
			CS = Local * CS;
		}
		Out.Fist.Add(Fist * SocketLocal * CS);
	}

	Out.Yaw = Art->HasField(TEXT("meshYaw")) ? float(Art->GetNumberField(TEXT("meshYaw"))) : -90.f;
	Out.MinZ = float(FMath::Min(Mesh->GetBounds().GetBox().Min.Z, 0.0));

	// A wrong frame (a clip from another skeleton, a socket on the wrong hand) would cut behind the body: fall back.
	const FQuat Turn(FRotator(0.f, Out.Yaw, 0.f));
	FVector Mean = FVector::ZeroVector;
	for (const FTransform& F : Out.Fist)
	{
		Mean += Turn.RotateVector(F.TransformPosition(FVector(60.f, 0.f, 0.f)));
	}
	Mean /= float(Out.Fist.Num());
	if (Mean.X <= 0.f)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Blade path: %s puts the blade behind the body (mean %s); %s swings use the reach tube"), *Clip->GetName(), *Mean.ToCompactString(), *Key.ToString());
		return false;
	}
	UE_LOG(LogWildreach, Log, TEXT("Blade path: %s (%s %s %d) %d samples over %.2f-%.2f s, mean blade at %s"), *Clip->GetName(), *Sex.ToString(), *Key.ToString(), Variant, Out.Fist.Num(), T0, T1, *Mean.ToCompactString());
	return true;
}

bool FWRBladePaths::FistAt(FName Sex, FName Key, int32 Variant, float F, float HalfHeight, FTransform& Out)
{
	const FString CacheKey = FString::Printf(TEXT("%s/%s/%d"), *Sex.ToString(), *Key.ToString(), Variant);
	FPath* Path = Paths.Find(CacheKey);
	if (!Path)
	{
		Path = &Paths.Add(CacheKey);
		Path->bValid = Build(Sex, Key, Variant, *Path);
	}
	if (!Path->bValid || Path->Fist.Num() < 2)
	{
		return false;
	}
	const float X = FMath::Clamp(F, 0.f, 1.f) * float(Path->Fist.Num() - 1);
	const int32 A = FMath::Min(FMath::FloorToInt(X), Path->Fist.Num() - 2);
	FTransform Blend;
	Blend.Blend(Path->Fist[A], Path->Fist[A + 1], X - float(A));
	// The body's place on the capsule (WRCharacter.cpp body art: turned meshYaw, its lowest point on the capsule's foot).
	const FTransform MeshToActor(FRotator(0.f, Path->Yaw, 0.f), FVector(0.f, 0.f, -HalfHeight - Path->MinZ));
	Out = Blend * MeshToActor;
	return true;
}
