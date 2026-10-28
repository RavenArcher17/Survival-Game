#include "WRCreature.h"
#include "WRAnimInstance.h"
#include "WRBoss.h"
#include "Camera/PlayerCameraManager.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "GameFramework/GameStateBase.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRCombatLibrary.h"
#include "WRGameState.h"
#include "WRProjectile.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRSound.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "Dom/JsonObject.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- registry

float FWRCreatureDef::FoodValue(FName Item) const
{
	if (Item.IsNone())
	{
		return 0.f;
	}
	if (Item == TEXT("kibble"))
	{
		return WRTune(TEXT("taming.kibbleValue"), 2.f);
	}
	return Food.Contains(Item) ? 1.f : 0.f;
}

bool FWRCreatureDef::CanHunt(const FWRCreatureDef& Hunter, const FWRCreatureDef& Prey)
{
	if (Hunter.Diet != TEXT("carnivore") || Prey.Diet == TEXT("carnivore") || Prey.Archetype == TEXT("boss") || Prey.Archetype == TEXT("dummy")
		|| Prey.Role == TEXT("minion") || &Hunter == &Prey)
	{
		return false;
	}
	const float Ratio = Hunter.Archetype == TEXT("pack_hunter") ? WRTune(TEXT("creatureAI.packPreySizeRatio"), 2.f) : WRTune(TEXT("creatureAI.preySizeRatio"), 1.3f);
	return Prey.Radius <= Hunter.Radius * Ratio;
}

float FWRCreatureDef::TameSeconds() const
{
	const FString Key = FString::Printf(TEXT("taming.%sSeconds"), *Difficulty.ToString());
	return FWRTuning::Get().Num(Key, 300.f) / FMath::Max(0.1f, WRTune(TEXT("server.tamingMultiplier"), 1.f));
}

FWRCreatureRegistry& FWRCreatureRegistry::Get()
{
	static FWRCreatureRegistry Instance;
	return Instance;
}

FWRCreatureRegistry::FWRCreatureRegistry()
{
	Reload();
}

const FWRCreatureDef* FWRCreatureRegistry::Find(FName Id) const
{
	return Defs.FindByPredicate([Id](const FWRCreatureDef& D) { return D.Id == Id; });
}

static TArray<TPair<FName, int32>> ParseCounts(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
{
	TArray<TPair<FName, int32>> Out;
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (O->TryGetObjectField(Field, Obj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*Obj)->Values) Out.Add({ FName(*L.Key), static_cast<int32>(L.Value->AsNumber()) });
	}
	return Out;
}

static TArray<FName> ParseNames(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
{
	TArray<FName> Out;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (O->TryGetArrayField(Field, Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr) Out.Add(FName(*V->AsString()));
	}
	return Out;
}

void FWRCreatureRegistry::Reload()
{
	Defs.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("creatures"), List))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *List)
	{
		const TSharedPtr<FJsonObject> O = Value->AsObject();
		if (!O.IsValid() || !O->HasField(TEXT("id")))
		{
			continue;
		}
		FWRCreatureDef D;
		D.Id = FName(*O->GetStringField(TEXT("id")));
		D.Name = O->GetStringField(TEXT("name"));
		FString Str;
		if (O->TryGetStringField(TEXT("tier"), Str)) D.Tier = FName(*Str);
		if (O->TryGetStringField(TEXT("behavior"), Str)) D.Behavior = FName(*Str);
		if (O->TryGetStringField(TEXT("archetype"), Str)) D.Archetype = FName(*Str);
		if (O->TryGetStringField(TEXT("element"), Str)) D.Element = FName(*Str);
		if (O->TryGetStringField(TEXT("difficulty"), Str)) D.Difficulty = FName(*Str);
		if (O->TryGetStringField(TEXT("utility"), Str)) D.Utility = FName(*Str);
		O->TryGetStringField(TEXT("role"), D.Role);

		// Health and damage default to the zone tier's starting numbers.
		const FString TierKey = FString::Printf(TEXT("enemies.%s"), *D.Tier.ToString());
		D.Health = FWRTuning::Get().Num(TierKey + TEXT(".health"), 70.f);
		D.Damage = FWRTuning::Get().Num(TierKey + TEXT(".damage"), 12.f);
		double Num = 0.0;
		if (O->TryGetNumberField(TEXT("health"), Num)) D.Health = Num;
		if (O->TryGetNumberField(TEXT("damage"), Num)) D.Damage = Num;
		if (O->TryGetNumberField(TEXT("speed"), Num)) D.Speed = Num;
		if (O->TryGetNumberField(TEXT("poise"), Num)) D.Poise = Num;
		if (O->TryGetNumberField(TEXT("radius"), Num)) D.Radius = Num;
		if (O->TryGetNumberField(TEXT("halfHeight"), Num)) D.HalfHeight = Num;
		if (O->TryGetNumberField(TEXT("aggroRadius"), Num)) D.AggroRadius = Num;
		if (O->TryGetNumberField(TEXT("attackRange"), Num)) D.AttackRange = Num;
		if (O->TryGetNumberField(TEXT("rideSpeed"), Num)) D.RideSpeed = Num;
		O->TryGetBoolField(TEXT("mount"), D.bMount);
		O->TryGetBoolField(TEXT("nest"), D.bNest);
		O->TryGetBoolField(TEXT("hearing"), D.bHearing);

		const TArray<TSharedPtr<FJsonValue>>* Color = nullptr;
		if (O->TryGetArrayField(TEXT("color"), Color) && Color->Num() >= 3)
		{
			D.Color = FLinearColor((*Color)[0]->AsNumber(), (*Color)[1]->AsNumber(), (*Color)[2]->AsNumber());
		}
		D.Resists = ParseNames(O, TEXT("resists"));
		O->TryGetStringField(TEXT("mesh"), D.Mesh);
		if (O->TryGetNumberField(TEXT("meshYaw"), Num)) D.MeshYaw = Num;
		if (O->TryGetNumberField(TEXT("meshScale"), Num)) D.MeshScale = Num;
		if (O->TryGetNumberField(TEXT("glow"), Num)) D.Glow = Num;
		// Optional animation overrides (normally measured from the clips): paces in cm/s at play rate 1, where the
		// attack clip's strike lands (0..1), the bone the attack layer starts from.
		const TSharedPtr<FJsonObject>* PaceObj = nullptr;
		if (O->TryGetObjectField(TEXT("pace"), PaceObj))
		{
			if ((*PaceObj)->TryGetNumberField(TEXT("walk"), Num)) D.WalkPace = Num;
			if ((*PaceObj)->TryGetNumberField(TEXT("run"), Num)) D.RunPace = Num;
		}
		if (O->TryGetNumberField(TEXT("strikeAt"), Num)) D.StrikeAt = Num;
		if (O->TryGetStringField(TEXT("animUpper"), Str)) D.AnimUpper = FName(*Str);
		const TSharedPtr<FJsonObject>* AnimObj = nullptr;
		if (O->TryGetObjectField(TEXT("anims"), AnimObj))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& A : (*AnimObj)->Values) D.Anims.Add(FName(*A.Key), A.Value->AsString());
		}
		const TArray<TSharedPtr<FJsonValue>>* MatArr = nullptr;
		if (O->TryGetArrayField(TEXT("materials"), MatArr))
		{
			for (const TSharedPtr<FJsonValue>& M : *MatArr) D.Materials.Add(M->AsString());
		}
		D.TamingMethods = ParseNames(O, TEXT("taming"));
		D.Food = ParseNames(O, TEXT("food"));
		D.Products = ParseCounts(O, TEXT("products"));
		D.Attacks = ParseNames(O, TEXT("attacks"));
		D.Diet = O->TryGetStringField(TEXT("diet"), Str) ? FName(*Str)
			: D.Food.Contains(TEXT("raw_meat")) ? FName(TEXT("carnivore"))
			: D.Food.ContainsByPredicate([](const FName& F) { return F.ToString().EndsWith(TEXT("_ore")); }) ? FName(TEXT("lithovore"))
			: D.Food.Num() > 0 ? FName(TEXT("herbivore")) : NAME_None;
		D.Activity = O->TryGetStringField(TEXT("activity"), Str) ? FName(*Str) : FName(TEXT("any"));
		if (O->HasField(TEXT("loot")))
		{
			D.Loot = ParseCounts(O, TEXT("loot"));
		}
		else if (D.Archetype != TEXT("dummy"))
		{
			D.Loot = { { TEXT("raw_meat"), 2 }, { TEXT("hide"), 1 } };
		}
		Defs.Add(D);
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d creature definitions"), Defs.Num());
}

// ---------------------------------------------------------------- creature

static APlayerState* FindPlayerByName(const UWorld* World, const FString& Name)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS)
	{
		for (APlayerState* P : GS->PlayerArray)
		{
			if (P && P->GetPlayerName() == Name) return P;
		}
	}
	return nullptr;
}

int32 AWRCreature::TameLimitFor(const APlayerState* PS)
{
	return FMath::RoundToInt(WRTune(TEXT("taming.tameLimit"), 2.f) + UWRLifeSkillsComponent::StatFor(PS, TEXT("extraTames")));
}

AWRCreature::AWRCreature(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UWRAttributeSet>(TEXT("Attributes"));

	AIControllerClass = AWRCreatureAI::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	WeaponMesh->SetVisibility(false);

	// Used only while a player rides this creature.
	RideBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("RideBoom"));
	RideBoom->SetupAttachment(RootComponent);
	RideBoom->TargetArmLength = 600.f;
	RideBoom->SocketOffset = FVector(0.f, 0.f, 150.f);
	RideBoom->bUsePawnControlRotation = true;
	RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
	RideCamera->SetupAttachment(RideBoom, USpringArmComponent::SocketName);

	Skel = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Skel"));
	Skel->SetupAttachment(RootComponent);
	Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skel->SetVisibility(false);
	Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetVisibility(false);
	GlowLight->SetIntensity(8000.f);
	GlowLight->SetAttenuationRadius(1500.f);
	GlowLight->SetLightColor(FLinearColor(1.f, 0.85f, 0.5f));
}

AWRCreature* AWRCreature::SpawnCreature(UWorld* World, FName CreatureId, const FVector& Location, const FRotator& Rotation)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	if (!World || !Def)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Unknown creature %s"), *CreatureId.ToString());
		return nullptr;
	}
	UClass* Class = Def->Archetype == TEXT("dummy") ? AWRTrainingDummy::StaticClass()
		: Def->Archetype == TEXT("boss") ? AWRBoss::StaticClass() : AWRCreature::StaticClass();
	const FTransform Transform(Rotation, Location + FVector(0.f, 0.f, FMath::Max(Def->HalfHeight, Def->Radius) + 5.f));
	AWRCreature* Creature = World->SpawnActorDeferred<AWRCreature>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Creature)
	{
		Creature->CreatureId = CreatureId;
		Creature->FinishSpawning(Transform);
	}
	return Creature;
}

void AWRCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCreature, CreatureId);
	DOREPLIFETIME(AWRCreature, TamedBy);
	DOREPLIFETIME(AWRCreature, TameOrder);
	DOREPLIFETIME(AWRCreature, bTameAggressive);
	DOREPLIFETIME(AWRCreature, Torpor);
	DOREPLIFETIME(AWRCreature, MaxTorpor);
	DOREPLIFETIME(AWRCreature, TameProgress);
	DOREPLIFETIME(AWRCreature, TameEffectiveness);
	DOREPLIFETIME(AWRCreature, TameBonus);
	DOREPLIFETIME(AWRCreature, TameFoodCount);
	DOREPLIFETIME(AWRCreature, TameFoodItem);
	DOREPLIFETIME(AWRCreature, TameEta);
	DOREPLIFETIME(AWRCreature, TameFoodNeeded);
	DOREPLIFETIME(AWRCreature, MealLeft);
	DOREPLIFETIME(AWRCreature, WakeIn);
	DOREPLIFETIME(AWRCreature, bAsleep);
	DOREPLIFETIME(AWRCreature, AggroRep);
	DOREPLIFETIME(AWRCreature, Rider);
}

void AWRCreature::ApplyDef()
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def)
	{
		return;
	}
	GetCapsuleComponent()->SetCapsuleSize(Def->Radius, Def->HalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = Def->Speed;
	GetCharacterMovement()->MaxFlySpeed = Def->Speed;
	BaseBodyColor = Def->Color;
	SetBodyColor(Def->Color);

	// Glowing creatures without art (the wisp): an emissive sphere in their colour that lights its surroundings.
	if (Def->Glow > 0.f && Def->Mesh.IsEmpty() && GetNetMode() != NM_DedicatedServer)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow"));
		if (Sphere && GlowMat)
		{
			BodyMesh->SetStaticMesh(Sphere);
			BodyMaterial = BodyMesh->CreateDynamicMaterialInstance(0, GlowMat);
			BodyMaterial->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("creatures.glowEmissive"), 2.f));  // brighter washes out to white
			SetBodyColor(Def->Color);
			FaceMarker->SetVisibility(false);
		}
		GlowLight->SetLightColor(Def->Color);
		GlowLight->SetIntensity(Def->Glow);
		GlowLight->SetAttenuationRadius(WRTune(TEXT("creatures.glowRadius"), 700.f));
		GlowLight->SetVisibility(true);
	}

	// Real art: swap the placeholder shape for the skeletal mesh, fitted to the capsule with its feet on the ground.
	USkeletalMesh* ArtMesh = Def->Mesh.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *Def->Mesh);
	if (ArtMesh && GetNetMode() != NM_DedicatedServer)
	{
		Skel->SetSkeletalMesh(ArtMesh);
		for (int32 Slot = 0; Slot < Def->Materials.Num(); ++Slot)
		{
			if (UMaterialInterface* Mat = Def->Materials[Slot].IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Def->Materials[Slot]))
			{
				Skel->SetMaterial(Slot, Mat);
			}
		}
		const FBox Box = ArtMesh->GetBounds().GetBox();
		const float Height = FMath::Max(1.f, Box.Max.Z - Box.Min.Z);
		const float Scale = Def->HalfHeight * 2.f / Height * Def->MeshScale;
		Skel->SetRelativeScale3D(FVector(Scale));
		// The engine grows a capsule whose half-height is under its radius, so stand on its real bottom.
		Skel->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Box.Min.Z * Scale));
		Skel->SetRelativeRotation(FRotator(0.f, Def->MeshYaw, 0.f));
		Skel->SetVisibility(true);
		BodyMesh->SetVisibility(false);
		FaceMarker->SetVisibility(false);
		AnimAssets.Reset();
		for (const TPair<FName, FString>& A : Def->Anims)
		{
			if (UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *A.Value)) AnimAssets.Add(A.Key, Anim);
		}
		// The layered animation instance (no animation blueprint), set up after the scale so it paces in cm.
		Skel->SetAnimInstanceClass(UWRAnimInstance::StaticClass());
		SetupCreatureAnim(*Def);
	}
}

namespace WRCreatureAnimDetail
{
	UAnimSequenceBase* Clip(const TMap<FName, TObjectPtr<UAnimSequenceBase>>& Assets, FName Key)
	{
		const TObjectPtr<UAnimSequenceBase>* Found = Assets.Find(Key);
		return Found ? Found->Get() : nullptr;
	}

	/** Within this many cm of a local player's view (its footsteps are worth hearing). */
	bool NearLocalView(const AActor* Actor, float Range)
	{
		for (FConstPlayerControllerIterator It = Actor->GetWorld()->GetPlayerControllerIterator(); It; ++It)
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

UWRAnimInstance* AWRCreature::CreatureAnim() const
{
	return Cast<UWRAnimInstance>(Skel->GetAnimInstance());
}

void AWRCreature::SetupCreatureAnim(const FWRCreatureDef& Def)
{
	UWRAnimInstance* Anim = CreatureAnim();
	if (!Anim)
	{
		return;
	}
	using WRCreatureAnimDetail::Clip;
	Anim->ResetSetup();
	// Walk and run paced to the ground (measured from their feet unless creatures.json gives a pace), over the idle.
	TArray<FWRLocoClipDesc> Ground;
	UAnimSequenceBase* Walk = Clip(AnimAssets, TEXT("walk"));
	UAnimSequenceBase* Run = Clip(AnimAssets, TEXT("run"));
	if (Walk) Ground.Add({ Walk, 0.f, 0, Def.WalkPace });
	if (Run && Run != Walk) Ground.Add({ Run, 0.f, 1, Def.RunPace });
	Anim->AddLocoSet(TEXT("ground"), Clip(AnimAssets, TEXT("idle")), Ground);
	// Flyers in the air: a wing beat at its own rate (no feet to pace it by), hovering when slow.
	if (UAnimSequenceBase* Fly = Clip(AnimAssets, TEXT("fly")))
	{
		UAnimSequenceBase* Hover = Clip(AnimAssets, TEXT("hover"));
		Anim->AddLocoSet(TEXT("fly"), Hover ? Hover : Clip(AnimAssets, TEXT("idle")), { { Fly, 0.f, 0, -1.f } });
	}
	Anim->SetMeshForward(FRotator(0.f, -Def.MeshYaw, 0.f).RotateVector(FVector::ForwardVector));
	// Attacks and flinches play from the neck up (a humanoid's spine), so one biting on the run keeps running.
	FName Upper = Def.AnimUpper;
	if (Upper.IsNone() && Skel->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Ref = Skel->GetSkeletalMeshAsset()->GetRefSkeleton();
		if (Ref.FindBoneIndex(TEXT("Spine")) != INDEX_NONE && Ref.FindBoneIndex(TEXT("LeftUpLeg")) != INDEX_NONE)
		{
			Upper = TEXT("Spine");  // a Mixamo humanoid
		}
		for (int32 Bone = 0; Bone < Ref.GetNum() && Upper.IsNone(); ++Bone)
		{
			if (Ref.GetBoneName(Bone).ToString().ToLower().Contains(TEXT("neck")))
			{
				Upper = Ref.GetBoneName(Bone);
			}
		}
	}
	Anim->SetUpperBody(Upper, {});
	OneShotUntil = 0.0;
	LastYaw = GetActorRotation().Yaw;
	UE_LOG(LogWildreach, Verbose, TEXT("Creature anim %s: walk %.0f run %.0f cm/s (moves at %.0f), attacks from %s"), *Def.Id.ToString(),
		Anim->LocoSpeed(TEXT("ground"), 0, 0.f), Anim->LocoSpeed(TEXT("ground"), 1, 0.f), Def.Speed, *Upper.ToString());
}

void AWRCreature::PlayCreatureOneShot(FName Key)
{
	UWRAnimInstance* Anim = CreatureAnim();
	UAnimSequenceBase* Clip = WRCreatureAnimDetail::Clip(AnimAssets, Key);
	if (Anim && Clip && Skel->IsVisible() && IsAlive())
	{
		OneShotUntil = GetWorld()->GetTimeSeconds() + Anim->PlayOneShot(Clip, EWRAnimLayer::Upper, 1.f);
	}
}

void AWRCreature::TickCreatureAnim(float DeltaSeconds)
{
	UWRAnimInstance* Anim = CreatureAnim();
	if (!Anim || !Skel->IsVisible())
	{
		return;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Yaw = GetActorRotation().Yaw;
	const float YawRate = DeltaSeconds > 0.f ? FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Yaw)) / DeltaSeconds : 0.f;
	LastYaw = Yaw;
	const bool bAlive = IsAlive();
	FWRAnimInput& In = Anim->Input;
	In.Speed = bAlive ? GetVelocity().Size2D() : 0.f;
	In.Angle = 0.f;  // forward clips only: it faces where it goes
	In.ActorYaw = Yaw;
	In.bTurnInPlace = false;
	In.bGrounded = Move->IsMovingOnGround();
	In.Set = Move->IsFlying() && AnimAssets.Contains(TEXT("fly")) ? FName(TEXT("fly")) : FName(TEXT("ground"));
	// Turning on the spot, the feet step round (its turn rate x its radius) instead of spinning on still feet.
	const float Step = bAlive && In.Speed < 60.f ? FMath::DegreesToRadians(FMath::Min(YawRate, 360.f)) * GetSimpleCollisionRadius() * WRTune(TEXT("anim.creatureTurnStep"), 1.f) : 0.f;
	In.TurnStepSpeed = FMath::FInterpTo(In.TurnStepSpeed, Step, DeltaSeconds, 8.f);  // replicated turns arrive in steps
	In.IdleRate = bAsleep ? WRTune(TEXT("anim.sleepIdleRate"), 0.45f) : 1.f;  // slow breathing asleep
	// Within earshot the pose updates even unseen (its footsteps come from the foot phase); further off only when seen.
	EarshotClock -= DeltaSeconds;
	if (EarshotClock <= 0.f)
	{
		EarshotClock = 0.5f;
		Skel->VisibilityBasedAnimTickOption = WRCreatureAnimDetail::NearLocalView(this, WRTune(TEXT("anim.footstepRange"), 3000.f))
			? EVisibilityBasedAnimTickOption::AlwaysTickPose : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
}

void AWRCreature::OnSwingVisual(EWRSwingKind Kind, float Duration)
{
	StartTelegraph(Kind, Duration);  // art or placeholder body alike
	UWRAnimInstance* Anim = CreatureAnim();
	if (!Skel->IsVisible() || !Anim)
	{
		return;
	}
	// Creatures with several attack clips (attack, attack2, attack3) take them in turn.
	static const FName Keys[] = { TEXT("attack"), TEXT("attack2"), TEXT("attack3") };
	int32 Count = 0;
	while (Count < 3 && AnimAssets.Contains(Keys[Count])) ++Count;
	UAnimSequenceBase* Clip = WRCreatureAnimDetail::Clip(AnimAssets, Keys[Count > 0 ? AttackVariant++ % Count : 0]);
	if (!Clip)
	{
		return;
	}
	// When the damage lands, in seconds from the swing's start (the server's timing for each move).
	const TCHAR* Move = TEXT("heavy");
	float Hit = Duration - 0.2f;  // a boss's attack resolves 0.2 s before its swing ends
	switch (Kind)
	{
	case EWRSwingKind::Bite:   Move = TEXT("bite");   Hit = Duration - WRTune(TEXT("creature.biteTiming.active"), 0.15f); break;  // sweeps after its windup
	case EWRSwingKind::Snap:   Move = TEXT("snap");   Hit = WRTune(TEXT("anim.creatureAttack.snap.hit"), 0.12f); break;     // at running prey: at once
	case EWRSwingKind::Lunge:  Move = TEXT("lunge");  Hit = Duration + WRTune(TEXT("anim.creatureAttack.lunge.hit"), 0.25f); break;   // mid-leap
	case EWRSwingKind::Charge: Move = TEXT("charge"); Hit = Duration + WRTune(TEXT("anim.creatureAttack.charge.hit"), 0.5f); break;  // into the run
	case EWRSwingKind::Stomp:  Move = TEXT("stomp");  Hit = Duration; break;
	case EWRSwingKind::Spit:   Move = TEXT("spit");   Hit = Duration; break;
	default: break;
	}
	Hit = FMath::Max(Hit, 0.05f);
	const FWRCreatureDef* Def = GetDef();
	const float Strike = Clip->GetPlayLength() * (Def && Def->StrikeAt > 0.f ? Def->StrikeAt : Anim->StrikeFraction(Clip));
	// Each move can have its own pace (a slow heavy stomp, a quick spit); otherwise the clip is fitted so its strike
	// lands with the damage. Too slow to get there in time, it starts partway in; too quick, it holds its first frame.
	float Rate = FWRTuning::Get().Num(FString::Printf(TEXT("anim.creatureAttack.%s.rate"), Move), 0.f);
	Rate = FMath::Clamp(Rate > 0.f ? Rate : Strike / Hit, WRTune(TEXT("anim.creatureAttackMinRate"), 0.4f), WRTune(TEXT("anim.creatureAttackMaxRate"), 2.5f));
	const float Start = FMath::Max(0.f, Strike - Rate * Hit);
	const float Delay = FMath::Max(0.f, Hit - (Strike - Start) / Rate);
	OneShotUntil = GetWorld()->GetTimeSeconds() + Anim->PlayOneShot(Clip, EWRAnimLayer::Upper, Rate, Start, Delay);
}

namespace WRTelegraphDetail
{
	/** Seconds from a swing's start to the moment its move lands (its wind-up), by kind; 0 = nothing to telegraph. */
	float WindupOf(EWRSwingKind Kind, float Duration)
	{
		switch (Kind)
		{
		case EWRSwingKind::Bite: return Duration - WRTune(TEXT("creature.biteTiming.active"), 0.15f);  // it sweeps after its wind-up
		case EWRSwingKind::Lunge:
		case EWRSwingKind::Charge:
		case EWRSwingKind::Stomp:
		case EWRSwingKind::Spit: return Duration;  // the move starts as its wind-up ends
		case EWRSwingKind::Heavy:
		case EWRSwingKind::WeaponArt: return Duration - 0.2f;  // a boss's attack resolves 0.2 s before its swing ends
		default: return 0.f;  // a snap at running prey lands at once (prey can't block or dodge)
		}
	}

	const TCHAR* MoveOf(EWRSwingKind Kind)
	{
		switch (Kind)
		{
		case EWRSwingKind::Bite: return TEXT("bite");
		case EWRSwingKind::Lunge: return TEXT("lunge");
		case EWRSwingKind::Charge: return TEXT("charge");
		case EWRSwingKind::Stomp: return TEXT("stomp");
		case EWRSwingKind::Spit: return TEXT("spit");
		default: return TEXT("heavy");
		}
	}
}

void AWRCreature::StartTelegraph(EWRSwingKind Kind, float Duration)
{
	const float Windup = WRTelegraphDetail::WindupOf(Kind, Duration);
	// A creature just arriving on this machine replays its last swing: that's not a new attack.
	if (GetNetMode() == NM_DedicatedServer || Windup < 0.05f || !IsAlive() || GetGameTimeSinceCreation() < 0.5f
		|| WRTune(TEXT("telegraph.enabled"), 1.f) <= 0.f)
	{
		return;
	}
	EndTelegraph();  // a new swing takes over from one still fading
	const double Now = GetWorld()->GetTimeSeconds();
	TelegraphStart = Now;
	TelegraphStrike = Now + Windup;
	TelegraphMove = FName(WRTelegraphDetail::MoveOf(Kind));

	// The glow: an overlay drawn over its own materials (the art, or the placeholder shape).
	UMeshComponent* GlowOn = Skel->IsVisible() ? static_cast<UMeshComponent*>(Skel.Get()) : static_cast<UMeshComponent*>(BodyMesh.Get());
	if (!TelegraphGlow)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_WindupGlow.M_WindupGlow")))
		{
			TelegraphGlow = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	if (TelegraphGlow && GlowOn && GlowOn->IsVisible())
	{
		// One warning colour for every creature (sRGB in tuning), so the cue is learned once.
		const FColor C(uint8(WRTune(TEXT("telegraph.color.r"), 255.f)), uint8(WRTune(TEXT("telegraph.color.g"), 60.f)), uint8(WRTune(TEXT("telegraph.color.b"), 20.f)));
		TelegraphGlow->SetVectorParameterValue(TEXT("Color"), FLinearColor(C));
		TelegraphGlow->SetScalarParameterValue(TEXT("Wash"), WRTune(TEXT("telegraph.wash"), 0.35f));
		GlowOn->SetOverlayMaterial(TelegraphGlow);
		TelegraphMesh = GlowOn;
	}
	// The rear-back moves the fitted art (bosses keep to their own big clips).
	const FWRCreatureDef* Def = GetDef();
	if (Skel->IsVisible() && Def && Def->Archetype != TEXT("boss"))
	{
		SkelBaseLocation = Skel->GetRelativeLocation();
		SkelBaseRotation = Skel->GetRelativeRotation();
		bTelegraphPosed = true;
	}
	WRSound::PlayOn(this, TEXT("creature.windup"));
	TickTelegraph();
	UE_LOG(LogWildreach, Verbose, TEXT("TELEGRAPH %s %s wind-up %.2f s"), *GetName(), *TelegraphMove.ToString(), Windup);
}

void AWRCreature::TickTelegraph()
{
	if (TelegraphStart < 0.0)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Windup = FMath::Max(0.05f, float(TelegraphStrike - TelegraphStart));
	const float Release = FMath::Max(0.05f, T.Num(TEXT("telegraph.releaseSeconds"), 0.2f));
	// Over once the move has landed, or if it's cut short (staggered by a parry, frozen, knocked out, dead).
	if (Now > TelegraphStrike + Release || !IsAlive() || HasTag(WRTags::Status_Staggered) || HasTag(WRTags::Status_Frozen) || HasTag(WRTags::Status_Unconscious))
	{
		EndTelegraph();
		return;
	}
	float Glow = 0.f, Pose = 0.f;
	if (Now <= TelegraphStrike)
	{
		const float A = float(Now - TelegraphStart) / Windup;
		// A flash as it starts (catches the eye), then building to its brightest just before the move.
		const float Flash = FMath::Max(0.f, 1.f - float(Now - TelegraphStart) / T.Num(TEXT("telegraph.flashSeconds"), 0.12f));
		Glow = FMath::Max(FMath::Lerp(T.Num(TEXT("telegraph.startGlow"), 0.7f), 1.f, A * A), Flash);
		// Reared back by telegraph.poseBy of the wind-up (eased), then held, coiled, until the move.
		Pose = 1.f - FMath::Square(1.f - FMath::Min(1.f, A / FMath::Max(0.1f, T.Num(TEXT("telegraph.poseBy"), 0.6f))));
	}
	else
	{
		// The move: the glow goes out and the body springs forward past rest, then settles.
		const float U = float(Now - TelegraphStrike) / Release;
		Glow = FMath::Square(1.f - U);
		Pose = (1.f - U) * (1.f - 2.2f * U);
	}
	if (TelegraphGlow)
	{
		TelegraphGlow->SetScalarParameterValue(TEXT("Glow"), Glow * T.Num(TEXT("telegraph.glow"), 5.f));
		TelegraphGlow->SetScalarParameterValue(TEXT("Opacity"), Glow * T.Num(TEXT("telegraph.opacity"), 0.75f));
	}
	if (bTelegraphPosed)
	{
		// Nose up pivots on the hind feet (rearing), head down on the front ones (a charge lowering its horns); back and
		// sink shift the weight (fractions of its radius / half height).
		const FString Key = FString::Printf(TEXT("telegraph.%s."), *TelegraphMove.ToString());
		const float R = GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float Pitch = T.Num(Key + TEXT("pitch"), 0.f) * Pose;
		const FVector Pivot(Pitch >= 0.f ? -R : R, 0.f, -HH);
		const FQuat Q(FRotator(Pitch, 0.f, 0.f));
		const FVector Shift(-T.Num(Key + TEXT("back"), 0.f) * R, 0.f, -T.Num(Key + TEXT("sink"), 0.f) * HH);
		Skel->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(SkelBaseLocation - Pivot) + Shift * Pose, Q * FQuat(SkelBaseRotation));
	}
}

void AWRCreature::EndTelegraph()
{
	if (TelegraphMesh)
	{
		TelegraphMesh->SetOverlayMaterial(nullptr);
		TelegraphMesh = nullptr;
	}
	if (bTelegraphPosed)
	{
		Skel->SetRelativeLocationAndRotation(SkelBaseLocation, SkelBaseRotation);
		bTelegraphPosed = false;
	}
	TelegraphStart = TelegraphStrike = -1.0;
}

void AWRCreature::OnHitVisual(EWRHitResult Result)
{
	// A flinch, unless it's mid-attack (or dying: the death takes over).
	UWRAnimInstance* Anim = CreatureAnim();
	UAnimSequenceBase* Clip = WRCreatureAnimDetail::Clip(AnimAssets, TEXT("hit"));
	if (Anim && Clip && Skel->IsVisible() && Result == EWRHitResult::Hit && IsAlive() && GetWorld()->GetTimeSeconds() >= OneShotUntil)
	{
		OneShotUntil = GetWorld()->GetTimeSeconds() + Anim->PlayOneShot(Clip, EWRAnimLayer::Upper, 1.f);
	}
}

void AWRCreature::OnDeathVisual()
{
	UWRAnimInstance* Anim = CreatureAnim();
	if (Skel->IsVisible() && Anim)
	{
		Anim->PlayOneShot(WRCreatureAnimDetail::Clip(AnimAssets, TEXT("death")), EWRAnimLayer::Full, 1.f, 0.f, 0.f, true);
		OneShotUntil = 1.0e12;  // stay down
	}
}

void AWRCreature::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	const FWRCreatureDef* Def = GetDef();
	if (HasAuthority() && !Rider.IsValid() && Def && Def->Archetype != TEXT("boss") && !GetCharacterMovement()->IsFlying()
		&& WRTune(TEXT("anim.creatureArcTurns"), 1.f) > 0.f && !WorldDirection.IsNearlyZero())
	{
		// The way it walks may lead its facing by only so much, so a change of direction becomes a curve (its facing
		// turns at RotationRate) rather than a slide sideways while the body spins.
		const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
		const FVector Want = WorldDirection.GetSafeNormal2D();
		const float Turn = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Facing, Want).Z, FVector::DotProduct(Facing, Want)));
		const float Lead = WRTune(TEXT("anim.creatureLeadDegrees"), 60.f);
		if (!Facing.IsNearlyZero() && FMath::Abs(Turn) > Lead)
		{
			WorldDirection = Facing.RotateAngleAxis(FMath::Sign(Turn) * Lead, FVector::UpVector) * WorldDirection.Size();
			if (FMath::Abs(Turn) > WRTune(TEXT("anim.creatureSpinAngle"), 110.f) && GetVelocity().Size2D() < Def->Speed * 0.3f)
			{
				ScaleValue *= WRTune(TEXT("anim.creatureSpinCreep"), 0.15f);  // nearly stopped, facing well away: turn on the spot first
			}
		}
	}
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AWRCreature::OnRep_CreatureId()
{
	ApplyDef();
}

void AWRCreature::OnRep_Tamed()
{
	const FWRCreatureDef* Def = GetDef();
	GlowLight->SetVisibility(Def && ((IsTamed() && Def->Utility == TEXT("light")) || (Def->Glow > 0.f && Def->Mesh.IsEmpty())));
}

void AWRCreature::BeginPlay()
{
	Skel->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);  // animate from this frame's movement
	ApplyDef();
	Super::BeginPlay();
	Home = GetActorLocation();
	AbilitySystem->InitAbilityActorInfo(this, this);
	OnRep_Tamed();

	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def)
	{
		return;
	}
	InitVitals(Def->Health, 100.f, Def->Poise);
	MaxTorpor = Def->Health * WRTune(TEXT("taming.torporPerHealth"), 1.f);
	HungryAt = GetWorld()->GetTimeSeconds() + FMath::FRand() * WRTune(TEXT("creatureAI.huntIntervalMinutes"), 5.f) * 60.f;
	Weapon.FlatDamage = Def->Damage;
	if (Def->Damage > 0.f)
	{
		GrantAbility(UWRGA_Bite::StaticClass());
	}
	for (const FName& Resist : Def->Resists)
	{
		const FGameplayTag Tag = Resist == TEXT("lava") ? WRTags::Resist_Lava : Resist == TEXT("ice") ? WRTags::Resist_Ice : FGameplayTag();
		if (Tag.IsValid())
		{
			AbilitySystem->AddLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::TagOnly);
		}
	}
}

bool AWRCreature::IsUnconscious() const
{
	return HasTag(WRTags::Status_Unconscious);
}

bool AWRCreature::IsHostileTo(const AWRCharacterBase* Other) const
{
	if (!Other || Other == this)
	{
		return false;
	}
	const FString OtherOwner = Other->GetTameOwner();
	const APlayerState* OtherPS = Other->GetPlayerState();
	if (!IsTamed())
	{
		// Wild: hostile to survivors and tames; other wild creatures only in a hunt (predator and prey fight each other).
		if (Other->IsCreature() && OtherOwner.IsEmpty())
		{
			const AWRCreature* OtherCreature = Cast<AWRCreature>(Other);
			return OtherCreature && (HuntTarget.Get() == OtherCreature || OtherCreature->HuntTarget.Get() == this);
		}
		return true;
	}
	// Tamed: fights for its owner (and never against the owner's clan or their tames).
	if (OtherPS && FWRSocialRules::CanShare(TamedBy, OtherPS->GetPlayerName())) return false;
	if (!OtherOwner.IsEmpty() && FWRSocialRules::CanShare(TamedBy, OtherOwner)) return false;
	return true;
}

FString AWRCreature::GetDisplayLabel() const
{
	const FWRCreatureDef* Def = GetDef();
	const FString Name = Def ? Def->Name : GetName();
	return IsTamed() ? FString::Printf(TEXT("%s's %s"), *TamedBy, *Name) : bAsleep ? Name + TEXT(" (asleep)") : Name;
}

void AWRCreature::NotifyAttackedBy(AActor* Attacker)
{
	LastHitAt = GetWorld()->GetTimeSeconds();
	if (bAsleep || !IsTamed())
	{
		WokenUntil = LastHitAt + WRTune(TEXT("creatureAI.wokenSeconds"), 30.f);
		SetAsleep(false);
	}
	// Anything you hit fights back, unless it's a passive grazer (they run).
	const FWRCreatureDef* Def = GetDef();
	if (!IsTamed() && Def && Def->Behavior != TEXT("passive") && !AggroTarget.IsValid())
	{
		AggroTarget = Attacker;
	}
	if (!IsTamed() && Def && Def->Behavior == TEXT("neutral") && Spawner.IsValid())
	{
		// Neutral herds defend each other.
		Spawner->AlertPack(Attacker);
	}
	// Grazers bolt from whatever hurt them (wild ones; the hunt they were part of is over).
	if (!IsTamed() && Def && Def->Behavior == TEXT("passive") && !IsUnconscious())
	{
		StartFleeing(Attacker, TEXT("attacked"));
	}
	// Hitting a knocked-out creature (even with tranq ammo) costs taming effectiveness: feed it narcotics instead.
	if (IsUnconscious() && !IsTamed())
	{
		TameEffectiveness = FMath::Max(0.f, TameEffectiveness - WRTune(TEXT("taming.effectivenessPerHit"), 0.06f));
	}
	bCalm = false;
}

bool AWRCreature::IsHungry() const
{
	return GetWorld()->GetTimeSeconds() >= HungryAt && GetWorld()->GetTimeSeconds() >= EatingUntil;
}

bool AWRCreature::IsEating() const
{
	return GetWorld()->GetTimeSeconds() < EatingUntil;
}

bool AWRCreature::IsFleeing() const
{
	return GetWorld()->GetTimeSeconds() < FleeUntil;
}

bool AWRCreature::CanBoltAgain() const
{
	return GetWorld()->GetTimeSeconds() > FleeUntil + WRTune(TEXT("creatureAI.preyRestSeconds"), 4.f);
}

bool AWRCreature::RecentPackLoss() const
{
	return GetWorld()->GetTimeSeconds() - PackLossAt < WRTune(TEXT("creatureAI.packLossSeconds"), 15.f);
}

float AWRCreature::SinceHit() const
{
	return static_cast<float>(GetWorld()->GetTimeSeconds() - LastHitAt);
}

bool AWRCreature::WasWokenRecently() const
{
	return GetWorld()->GetTimeSeconds() < WokenUntil;
}

void AWRCreature::StartFleeing(AActor* From, const TCHAR* Why)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def || IsTamed() || Def->Archetype == TEXT("boss") || Def->Archetype == TEXT("dummy") || Def->Role == TEXT("minion"))
	{
		return;
	}
	const bool bWas = IsFleeing();
	FleeUntil = GetWorld()->GetTimeSeconds() + WRTune(TEXT("creatureAI.fleeSeconds"), 8.f);
	FleeFrom = From;
	AggroTarget = nullptr;
	HuntTarget = nullptr;
	SetAsleep(false);
	if (!bWas)
	{
		const UWRAttributeSet* A = GetAttributes();
		UE_LOG(LogWildreach, Log, TEXT("AI %s flees from %s (%s, %.0f%% health)"), *GetName(), *GetNameSafe(From), Why,
			A ? 100.f * A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 0.f);
	}
}

void AWRCreature::NotePackLoss()
{
	PackLossAt = GetWorld()->GetTimeSeconds();
}

void AWRCreature::Ate(AWRCreature* Prey)
{
	const float Now = GetWorld()->GetTimeSeconds();
	EatingUntil = Now + WRTune(TEXT("creatureAI.eatSeconds"), 8.f);
	HungryAt = EatingUntil + WRTune(TEXT("creatureAI.huntIntervalMinutes"), 5.f) * 60.f * FMath::FRandRange(0.75f, 1.25f);
	HuntTarget = nullptr;
	AggroTarget = nullptr;
	UE_LOG(LogWildreach, Log, TEXT("AI %s eats the %s it killed"), *GetName(), *GetNameSafe(Prey));
}

void AWRCreature::HuntFailed()
{
	if (HuntTarget.IsValid())
	{
		UE_LOG(LogWildreach, Log, TEXT("AI %s gives up on %s"), *GetName(), *GetNameSafe(HuntTarget.Get()));
	}
	HuntTarget = nullptr;
	HungryAt = GetWorld()->GetTimeSeconds() + WRTune(TEXT("creatureAI.huntRetrySeconds"), 45.f);
}

void AWRCreature::SetAsleep(bool bSleep)
{
	if (bAsleep != bSleep && HasAuthority())
	{
		bAsleep = bSleep;
		UE_LOG(LogWildreach, Log, TEXT("AI %s %s"), *GetName(), bSleep ? TEXT("falls asleep") : TEXT("wakes up"));
	}
}

void AWRCreature::AddTorpor(float Amount)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || IsTamed() || !Def || !Def->CanTameBy(TEXT("knockout_feed")) || Amount <= 0.f)
	{
		return;
	}
	Torpor = FMath::Min(MaxTorpor, Torpor + Amount);
	if (Torpor >= MaxTorpor && !IsUnconscious())
	{
		if (TameProgress <= 0.f)
		{
			TameEffectiveness = 1.f;  // a fresh knockout starts a fresh tame
		}
		Status->ApplyStatus(WRTags::Status_Unconscious, 100000.f);
		AbilitySystem->CancelAllAbilities();
		AggroTarget = nullptr;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		UE_LOG(LogWildreach, Log, TEXT("TAME %s knocked out"), *GetName());
	}
}

bool AWRCreature::TryPathMagic(const FWRHit& Hit)
{
	const FWRCreatureDef* Def = GetDef();
	const AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	const APlayerState* PS = Attacker ? Attacker->GetPlayerState() : nullptr;
	if (!HasAuthority() || IsTamed() || !Def || !PS || !Def->CanTameBy(TEXT("path_magic")) || Hit.PathId.IsNone() || Hit.PathId != Def->Element)
	{
		return false;
	}
	// Your path's element calms a creature of the same element.
	TamerName = PS->GetPlayerName();
	bCalm = true;
	AggroTarget = nullptr;
	FedUntil = GetWorld()->GetTimeSeconds() + 15.f;
	TameProgress = FMath::Min(1.f, TameProgress + WRTune(TEXT("taming.pathMagicPerHit"), 0.08f));
	UE_LOG(LogWildreach, Log, TEXT("TAME %s calmed by %s's path magic (%.0f%%)"), *GetName(), *TamerName, TameProgress * 100.f);
	if (TameProgress >= 1.f)
	{
		CompleteTame(TamerName);
	}
	return true;
}

FString AWRCreature::Feed(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRCreatureDef* Def = GetDef();
	if (!PS || !Def || !HasAuthority() || IsTamed())
	{
		return TEXT("can't feed that");
	}
	const bool bKnockout = Def->CanTameBy(TEXT("knockout_feed")) && IsUnconscious();
	const bool bOffer = Def->CanTameBy(TEXT("offer_food")) && !AggroTarget.IsValid();
	if (!bKnockout && !bOffer)
	{
		if (Def->CanTameBy(TEXT("knockout_feed"))) return FString::Printf(TEXT("Knock the %s out first (heavy hits, frost, torpor darts)"), *Def->Name);
		if (Def->CanTameBy(TEXT("steal_egg"))) return FString::Printf(TEXT("%s are tamed from eggs: steal one from a nest"), *Def->Name);
		if (Def->CanTameBy(TEXT("path_magic"))) return TEXT("Calm it with your path's element");
		return TEXT("It won't take food from you right now");
	}
	if (CountTames(GetWorld(), PS->GetPlayerName()) >= TameLimitFor(PS))
	{
		return TEXT("You can't keep any more tames");
	}
	// The best food you carry: kibble, then what it eats.
	TArray<FName> Foods = { FName(TEXT("kibble")) };
	Foods.Append(Def->Food);
	FName FoodUsed;
	for (const FName& Food : Foods)
	{
		if (FoodValue(Food) > 0.f && PS->GetInventory()->CountOf(Food) > 0) { FoodUsed = Food; break; }
	}
	if (FoodUsed.IsNone())
	{
		return FString::Printf(TEXT("%s eats %s (or kibble)"), *Def->Name, Def->Food.Num() > 0 ? *Def->Food[0].ToString() : TEXT("nothing you have"));
	}
	if (bKnockout)
	{
		return GiveTameFood(Player, FoodUsed);
	}
	// A calm creature takes it from your hand and keeps taming while it eats; kibble lasts twice as long.
	PS->GetInventory()->RemoveItem(FoodUsed, 1);
	TamerName = PS->GetPlayerName();
	TamerSpeed = 1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("tamingSpeed"));
	UWRLifeSkillsComponent::Award(PS, TEXT("taming"), TEXT("feed"), 5.f);
	bCalm = true;
	MealValue = 1.f;
	FedUntil = GetWorld()->GetTimeSeconds() + WRTune(TEXT("taming.secondsPerFood"), 30.f) * FoodValue(FoodUsed);
	return FString::Printf(TEXT("Fed %s: taming %.0f%% (needs %.0f s fed)"), *FoodUsed.ToString(), TameProgress * 100.f, Def->TameSeconds());
}

float AWRCreature::FoodValue(FName Item) const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? Def->FoodValue(Item) : 0.f;
}

FString AWRCreature::GiveTameFood(AWRPlayerController* Player, FName Item)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRCreatureDef* Def = GetDef();
	if (!PS || !Def || !HasAuthority() || IsTamed() || !Def->CanTameBy(TEXT("knockout_feed")) || !IsUnconscious())
	{
		return TEXT("Knock it out first");
	}
	if (CountTames(GetWorld(), PS->GetPlayerName()) >= TameLimitFor(PS))
	{
		return TEXT("You can't keep any more tames");
	}
	const FWRItemDef* Food = FWRItemRegistry::Get().Find(Item);
	const FString FoodName = Food ? Food->Name : Item.ToString();
	if (FoodValue(Item) <= 0.f)
	{
		return FString::Printf(TEXT("%s won't eat %s"), *Def->Name, *FoodName);
	}
	int32 Stashed = 0;
	for (const TPair<FName, int32>& Kept : TameStash)
	{
		Stashed += Kept.Value;
	}
	const int32 Room = FMath::RoundToInt(WRTune(TEXT("taming.stashMax"), 50.f)) - Stashed;
	const int32 Count = FMath::Min(PS->GetInventory()->CountOf(Item), Room);
	if (Count <= 0)
	{
		return Room <= 0 ? TEXT("Its food stash is full") : FString::Printf(TEXT("You have no %s"), *FoodName);
	}
	PS->GetInventory()->RemoveItem(Item, Count);
	TameStash.FindOrAdd(Item) += Count;
	TamerName = PS->GetPlayerName();
	TamerSpeed = 1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("tamingSpeed"));
	UWRLifeSkillsComponent::Award(PS, TEXT("taming"), TEXT("feed"), 5.f);
	UpdateTameForecast();
	UE_LOG(LogWildreach, Log, TEXT("TAME %s given %d %s by %s (%d in its stash)"), *GetName(), Count, *Item.ToString(), *TamerName, TameFoodCount);
	return FString::Printf(TEXT("Gave %d %s: %d food in its stash, taming %.0f%%, %s"), Count, *FoodName, TameFoodCount, TameProgress * 100.f,
		TameEta >= 0.f ? *FString::Printf(TEXT("tamed in ~%.0f s if it stays under"), TameEta) : *FString::Printf(TEXT("needs %d more"), TameFoodNeeded));
}

FString AWRCreature::GiveNarcotic(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority() || IsTamed() || !IsUnconscious())
	{
		return TEXT("Only a knocked-out creature takes narcotics");
	}
	if (PS->GetInventory()->CountOf(TEXT("narcotic")) < 1)
	{
		return TEXT("You have no narcotic (alchemy table: berries + raw meat)");
	}
	PS->GetInventory()->RemoveItem(TEXT("narcotic"), 1);
	Torpor = FMath::Min(MaxTorpor, Torpor + MaxTorpor * WRTune(TEXT("taming.narcoticTorpor"), 0.3f));
	UE_LOG(LogWildreach, Log, TEXT("TAME %s given a narcotic: torpor %.0f/%.0f"), *GetName(), Torpor, MaxTorpor);
	return FString::Printf(TEXT("Narcotic: torpor %.0f/%.0f"), Torpor, MaxTorpor);
}

void AWRCreature::UpdateTameForecast()
{
	const FWRCreatureDef* Def = GetDef();
	const float Seconds = Def ? FMath::Max(1.f, Def->TameSeconds()) : 1.f;
	const float PerFood = WRTune(TEXT("taming.secondsPerFood"), 30.f);
	const float Now = GetWorld()->GetTimeSeconds();
	MealLeft = FMath::Max(0.f, FedUntil - Now);
	const float Drain = MaxTorpor * WRTune(TEXT("taming.unconsciousTorporDecay"), 0.004f);
	WakeIn = Drain > 0.f ? Torpor / Drain : 0.f;
	// Meals still to come, best first (that's the order it eats them).
	TArray<TPair<float, int32>> Meals;
	TameFoodCount = 0;
	TameFoodItem = NAME_None;
	float BestValue = 0.f;
	for (const TPair<FName, int32>& Kept : TameStash)
	{
		const float Value = FoodValue(Kept.Key);
		Meals.Add({ Value, Kept.Value });
		TameFoodCount += Kept.Value;
		if (Value > BestValue)
		{
			BestValue = Value;
			TameFoodItem = Kept.Key;
		}
	}
	Meals.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key; });
	const float Rate = TamerSpeed / Seconds;  // progress per second per unit of food value
	float Left = 1.f - TameProgress - MealLeft * MealValue * Rate;
	float Eta = Left <= 0.f ? (1.f - TameProgress) / FMath::Max(KINDA_SMALL_NUMBER, MealValue * Rate) : MealLeft;
	for (const TPair<float, int32>& Meal : Meals)
	{
		for (int32 i = 0; i < Meal.Value && Left > 0.f; ++i)
		{
			const float Gain = Meal.Key * PerFood * Rate;
			Eta += Left >= Gain ? PerFood : PerFood * Left / Gain;
			Left -= Gain;
		}
	}
	TameEta = Left <= 0.f ? Eta : -1.f;
	const FName Next = !TameFoodItem.IsNone() ? TameFoodItem : (Def && Def->Food.Num() > 0 ? Def->Food[0] : NAME_None);
	const float NextGain = FoodValue(Next) * PerFood * Rate;
	TameFoodNeeded = Left > 0.f && NextGain > 0.f ? FMath::CeilToInt(Left / NextGain) : 0;
	if (TameFoodItem.IsNone())
	{
		TameFoodItem = Next;  // what to bring
	}
}

int32 AWRCreature::CountTames(const UWorld* World, const FString& PlayerName)
{
	int32 N = 0;
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		N += It->TamedBy == PlayerName && It->IsAlive() ? 1 : 0;
	}
	return N;
}

void AWRCreature::CompleteTame(const FString& PlayerName)
{
	TamedBy = PlayerName;
	TameOrder = EWRTameOrder::Follow;
	TameProgress = 1.f;
	Torpor = 0.f;
	AggroTarget = nullptr;
	Status->ClearStatus(WRTags::Status_Unconscious);
	ApplyTameBonus(TameEffectiveness * WRTune(TEXT("taming.bonusAtFull"), 0.5f));
	TameStash.Reset();
	TameFoodCount = 0;
	TameEta = -1.f;
	OnRep_Tamed();
	UWRLifeSkillsComponent::Award(FindPlayerByName(GetWorld(), PlayerName), TEXT("taming"), TEXT("tame"), 100.f);
	if (Spawner.IsValid())
	{
		// No longer part of the wild pack; the spawner will replace it.
		OnCreatureDied.Broadcast(this);
		Spawner = nullptr;
	}
	UE_LOG(LogWildreach, Log, TEXT("TAME %s tamed by %s (effectiveness %.0f%%: +%.0f%% health and bite)"), *GetName(), *PlayerName, TameEffectiveness * 100.f, TameBonus * 100.f);
}

void AWRCreature::ApplyTameBonus(float Bonus)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def)
	{
		return;
	}
	TameBonus = FMath::Max(0.f, Bonus);
	InitVitals(Def->Health * (1.f + TameBonus), 100.f, Def->Poise);
	Weapon.FlatDamage = Def->Damage * (1.f + TameBonus);
}

FString AWRCreature::Order(AWRPlayerController* Player, const FString& Command)
{
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || TamedBy != PS->GetPlayerName())
	{
		return TEXT("not your tame");
	}
	if (Command == TEXT("follow")) { TameOrder = EWRTameOrder::Follow; return TEXT("Following"); }
	if (Command == TEXT("stay")) { TameOrder = EWRTameOrder::Stay; Home = GetActorLocation(); return TEXT("Staying"); }
	if (Command == TEXT("aggressive")) { bTameAggressive = true; return TEXT("Will fight your enemies"); }
	if (Command == TEXT("passive")) { bTameAggressive = false; AggroTarget = nullptr; return TEXT("Won't fight"); }
	if (Command == TEXT("release"))
	{
		TamedBy.Reset();
		OnRep_Tamed();
		Home = GetActorLocation();
		return TEXT("Released into the wild");
	}
	if (Command == TEXT("ride"))
	{
		return Ride(Cast<AWRCharacter>(Player->GetPawn()));
	}
	return TEXT("unknown order");
}

FString AWRCreature::Ride(AWRCharacter* NewRider)
{
	const FWRCreatureDef* Def = GetDef();
	AController* PC = NewRider ? NewRider->GetController() : nullptr;
	if (!Def || !Def->bMount || !NewRider || !PC || Rider.IsValid())
	{
		return Def && !Def->bMount ? TEXT("can't ride that") : TEXT("can't ride now");
	}
	Rider = NewRider;
	NewRider->SetActorEnableCollision(false);
	NewRider->GetCharacterMovement()->DisableMovement();
	NewRider->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	NewRider->SetActorRelativeLocation(FVector(0.f, 0.f, Def->HalfHeight + 60.f));
	if (AController* AI = GetController())
	{
		AI->UnPossess();
		AI->Destroy();
	}
	PC->UnPossess();
	PC->Possess(this);
	GetCharacterMovement()->MaxWalkSpeed = Def->RideSpeed * (1.f + UWRLifeSkillsComponent::StatFor(NewRider, TEXT("rideSpeed")));
	UE_LOG(LogWildreach, Log, TEXT("TAME %s ridden by %s"), *GetName(), *NewRider->GetName());
	return TEXT("Riding (E to dismount)");
}

void AWRCreature::ServerDismount_Implementation()
{
	Dismount();
}

void AWRCreature::Dismount()
{
	AWRCharacter* Character = Rider.Get();
	AController* PC = GetController();
	if (!HasAuthority() || !Character)
	{
		return;
	}
	Rider = nullptr;
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocation(GetActorLocation() + GetActorRightVector() * (GetSimpleCollisionRadius() + 80.f) + FVector(0.f, 0.f, 50.f));
	Character->SetActorEnableCollision(true);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (PC)
	{
		PC->UnPossess();
		PC->Possess(Character);
	}
	if (const FWRCreatureDef* Def = GetDef())
	{
		GetCharacterMovement()->MaxWalkSpeed = Def->Speed;
	}
	SpawnDefaultController();
}

void AWRCreature::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!RideMapping)
	{
		RideMapping = NewObject<UInputMappingContext>(this);
		RideMoveAction = NewObject<UInputAction>(this);
		RideMoveAction->ValueType = EInputActionValueType::Axis2D;
		RideLookAction = NewObject<UInputAction>(this);
		RideLookAction->ValueType = EInputActionValueType::Axis2D;
		RideJumpAction = NewObject<UInputAction>(this);
		RideDismountAction = NewObject<UInputAction>(this);
		RideMapping->MapKey(RideMoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		FEnhancedActionKeyMapping& Back = RideMapping->MapKey(RideMoveAction, EKeys::S);
		Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		Back.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		RideMapping->MapKey(RideMoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(this));
		RideMapping->MapKey(RideMoveAction, EKeys::D);
		RideMapping->MapKey(RideLookAction, EKeys::Mouse2D);
		RideMapping->MapKey(RideJumpAction, EKeys::SpaceBar);
		RideMapping->MapKey(RideDismountAction, EKeys::E);
	}
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			Sub->AddMappingContext(RideMapping, 0);
		}
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(RideMoveAction, ETriggerEvent::Triggered, this, &AWRCreature::RideMove);
		Input->BindAction(RideLookAction, ETriggerEvent::Triggered, this, &AWRCreature::RideLook);
		Input->BindAction(RideJumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(RideDismountAction, ETriggerEvent::Started, this, &AWRCreature::ServerDismount);
	}
}

void AWRCreature::RideMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotationMatrix Yaw(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		AddMovementInput(Yaw.GetUnitAxis(EAxis::X), Axis.Y);
		AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), Axis.X);
	}
}

void AWRCreature::RideLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AWRCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickCreatureAnim(DeltaSeconds);
		TickTelegraph();
	}
	if (HasAuthority() && AggroRep != AggroTarget.Get())
	{
		AggroRep = AggroTarget.Get();   // sound only (WRAudioDirector): the AI keeps using AggroTarget
	}
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	const FWRCreatureDef* Def = GetDef();
	const float Now = GetWorld()->GetTimeSeconds();
	// Turning: slower on the spot (no whipping round), quicker on the move so its facing keeps up with its curve.
	// Ridden creatures keep the rider's quick turns; bosses keep their own rate.
	if (Def && !Rider.IsValid() && Def->Archetype != TEXT("boss"))
	{
		const float Moving = FMath::Clamp(GetVelocity().Size2D() / FMath::Max(1.f, Def->Speed * 0.5f), 0.f, 1.f);
		GetCharacterMovement()->RotationRate.Yaw = FMath::Lerp(WRTune(TEXT("anim.creatureSpinRate"), 160.f), WRTune(TEXT("anim.creatureTurnRate"), 260.f), Moving);
	}
	else if (Def && Rider.IsValid())
	{
		GetCharacterMovement()->RotationRate.Yaw = 400.f;
	}

	// Torpor wears off; an unconscious creature wakes when it reaches zero.
	if (Torpor > 0.f)
	{
		const float Rate = IsUnconscious() ? WRTune(TEXT("taming.unconsciousTorporDecay"), 0.004f) : WRTune(TEXT("taming.torporDecay"), 0.05f);
		Torpor = FMath::Max(0.f, Torpor - MaxTorpor * Rate * DeltaSeconds);
		if (Torpor <= 0.f && IsUnconscious())
		{
			// Awake before it was tamed: the tame starts over (the food it hasn't eaten stays in its stash).
			Status->ClearStatus(WRTags::Status_Unconscious);
			bCalm = false;
			FedUntil = 0.f;
			const float Was = TameProgress;
			TameProgress *= WRTune(TEXT("taming.wakeKeepsProgress"), 0.f);
			UE_LOG(LogWildreach, Log, TEXT("TAME %s woke up (%.0f%% tamed, %.0f%% kept)"), *GetName(), Was * 100.f, TameProgress * 100.f);
		}
	}

	// Knockout taming: it eats from its stash a meal at a time, best food first; plain food costs a little effectiveness.
	if (!IsTamed() && Def && IsUnconscious() && Now >= FedUntil && TameStash.Num() > 0)
	{
		FName Meal;
		for (const TPair<FName, int32>& Kept : TameStash)
		{
			if (Meal.IsNone() || FoodValue(Kept.Key) > FoodValue(Meal))
			{
				Meal = Kept.Key;
			}
		}
		if (--TameStash[Meal] <= 0)
		{
			TameStash.Remove(Meal);
		}
		MealValue = FoodValue(Meal);
		FedUntil = Now + WRTune(TEXT("taming.secondsPerFood"), 30.f);
		if (Meal != TEXT("kibble"))
		{
			TameEffectiveness = FMath::Max(0.f, TameEffectiveness - WRTune(TEXT("taming.effectivenessPerPlainMeal"), 0.01f));
		}
		UE_LOG(LogWildreach, Log, TEXT("TAME %s eats %s (effectiveness %.0f%%)"), *GetName(), *Meal.ToString(), TameEffectiveness * 100.f);
	}

	// Taming progresses while the creature is fed and either knocked out or calm.
	if (!IsTamed() && Def && !TamerName.IsEmpty() && Now < FedUntil && (IsUnconscious() || bCalm))
	{
		TameProgress = FMath::Min(1.f, TameProgress + DeltaSeconds * TamerSpeed * MealValue / FMath::Max(1.f, Def->TameSeconds()));
		if (TameProgress >= 1.f)
		{
			CompleteTame(TamerName);
		}
	}
	if (!IsTamed() && IsUnconscious())
	{
		UpdateTameForecast();
	}

	// Tame utilities and farm produce, once a minute.
	UtilityClock += DeltaSeconds;
	if (IsTamed() && Def && UtilityClock >= 60.f)
	{
		UtilityClock = 0.f;
		if (Def->Utility == TEXT("mine_ore") && TameOrder == EWRTameOrder::Follow)
		{
			for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
			{
				const APlayerState* PS = It->GetPlayerState();
				if (PS && PS->GetPlayerName() == TamedBy && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 2000.f)
				{
					if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(*It))
					{
						Inv->AddItem(Def->Id == TEXT("rimeshell") ? FName(TEXT("iron_ore")) : FName(TEXT("copper_ore")), 1);
					}
				}
			}
		}
	}
	ProduceClock += DeltaSeconds;
	if (IsTamed() && Def && Def->Products.Num() > 0)
	{
		const APlayerState* OwnerPS = FindPlayerByName(GetWorld(), TamedBy);
		const float Interval = WRTune(TEXT("husbandry.produceSeconds"), 600.f) / (1.f + UWRLifeSkillsComponent::StatFor(OwnerPS, TEXT("produceRate")));
		if (ProduceClock >= Interval)
		{
			ProduceClock = 0.f;
			const int32 Extra = FMath::RoundToInt(UWRLifeSkillsComponent::StatFor(OwnerPS, TEXT("produceExtra")));
			// Farm animals produce into an owned pen they stand in.
			for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
			{
				const FWRPieceDef* Piece = It->GetDef();
				if (Piece && Piece->Kind == TEXT("pen") && It->OwnerName == TamedBy && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 450.f)
				{
					if (It->Storage.Num() == 0) It->Storage.SetNum(10);
					for (const TPair<FName, int32>& P : Def->Products) FWRInventoryRules::Add(It->Storage, P.Key, P.Value + Extra);
					break;
				}
			}
		}
	}
}

void AWRCreature::OnDeath(AActor* Killer)
{
	// A kill feeds every hunter that was on it (the pack shares).
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->HuntTarget.Get() == this && It->IsAlive() && (*It == Killer || FVector::Dist(It->GetActorLocation(), GetActorLocation()) < WRTune(TEXT("creatureAI.shareKillRadius"), 1200.f)))
		{
			It->Ate(this);
		}
		else if (It->HuntTarget.Get() == this)
		{
			It->HuntTarget = nullptr;
			It->SetAggroTarget(nullptr);
		}
	}
	if (Spawner.IsValid() && !IsTamed())
	{
		Spawner->NotifyPackLoss(this);
	}
	OnCreatureDied.Broadcast(this);
	if (Rider.IsValid())
	{
		Dismount();
	}
	if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Killer))
	{
		if (const FWRCreatureDef* Def = GetDef())
		{
			// Gathering multiplier and Field dressing (more meat and hide).
			const float Mult = WRTune(TEXT("server.gatherMultiplier"), 1.f);
			const float Dressing = 1.f + UWRLifeSkillsComponent::StatFor(Killer, TEXT("creatureLoot"));
			for (const TPair<FName, int32>& L : Def->Loot)
			{
				const bool bCarcass = L.Key == TEXT("raw_meat") || L.Key == TEXT("hide");
				Inv->AddItem(L.Key, FMath::Max(1, FMath::RoundToInt(L.Value * Mult * (bCarcass ? Dressing : 1.f))));
			}
			const float TierXP = Def->Tier == TEXT("highland") ? 3.f : Def->Tier == TEXT("midland") ? 2.f : 1.f;
			UWRLifeSkillsComponent::Award(Killer, TEXT("tracking"), TEXT("kill"), 10.f, TierXP);
		}
	}
	if (IsTamed())
	{
		UE_LOG(LogWildreach, Log, TEXT("TAME %s (owned by %s) died for good, killed by %s"), *GetName(), *TamedBy, *GetNameSafe(Killer));
	}
	SetLifeSpan(4.f);
	if (AController* AI = GetController())
	{
		AI->UnPossess();
	}
}

// ---------------------------------------------------------------- dummy

AWRTrainingDummy::AWRTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CreatureId = TEXT("training_dummy");
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AWRTrainingDummy::NotifyAttackedBy(AActor* Attacker)
{
	LastHitTime = GetWorld()->GetTimeSeconds();
}

void AWRTrainingDummy::Die(AActor* Killer)
{
	// Dummies never die: they top themselves back up.
	if (AbilitySystem && Attributes)
	{
		UE_LOG(LogWildreach, Log, TEXT("DUMMY %s would have died (killed by %s)"), *GetName(), *GetNameSafe(Killer));
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	}
}

void AWRTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Attributes && Attributes->GetHealth() < Attributes->GetMaxHealth()
		&& GetWorld()->GetTimeSeconds() - LastHitTime > WRTune(TEXT("creature.dummyResetSeconds"), 3.f))
	{
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	}
}

// ---------------------------------------------------------------- AI

AWRCreatureAI::AWRCreatureAI()
{
	PrimaryActorTick.bCanEverTick = true;
	FlankAngle = FMath::FRandRange(-60.f, 60.f);
}

AActor* AWRCreatureAI::PickTarget(AWRCreature* Self) const
{
	const FWRCreatureDef* Def = Self->GetDef();
	if (!Def || Def->Behavior != TEXT("aggressive"))
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		// Survivors and tames (wild creatures are prey, picked separately).
		if (!It->IsAlive() || (It->IsCreature() && It->GetTameOwner().IsEmpty()) || !Self->IsHostileTo(*It) || !It->IsHostileTo(Self))
		{
			continue;
		}
		if (FWRSocialRules::IsInSafeZone(GetWorld(), It->GetActorLocation()))
		{
			continue;
		}
		float Aggro = Def->AggroRadius;
		// Asleep it notices you late; a night hunter at night notices you early.
		if (Self->IsAsleep()) Aggro *= WRTune(TEXT("creatureAI.sleepAggroMult"), 0.35f);
		else if (Def->Activity == TEXT("nocturnal") && AWRGameState::IsNightIn(Self)) Aggro *= WRTune(TEXT("creatureAI.nightAggroMult"), 1.4f);
		// Gloomhowls hunt by sound: sprinting survivors are heard from much further.
		if (Def->bHearing && It->GetVelocity().Size2D() > 600.f) Aggro *= 1.6f;
		// Sneaking / Calm presence: you're noticed later.
		Aggro *= FMath::Max(0.3f, 1.f + UWRLifeSkillsComponent::StatFor(*It, TEXT("aggroRadius")));
		const float Dist = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
		if (Dist >= Aggro && Dist < Aggro * 1.5f && It->GetPlayerState() && It->GetVelocity().Size2D() > 50.f && It->GetVelocity().Size2D() < 600.f)
		{
			// Sneaking XP: moving close to a predator that hasn't noticed you (checked 4x a second).
			UWRLifeSkillsComponent::Award(*It, TEXT("sneaking"), TEXT("sneak"), 0.25f);
		}
		if (Dist < Aggro && Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void AWRCreatureAI::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCreature* Self = Cast<AWRCreature>(GetPawn());
	if (!Self || !Self->IsAlive() || !Self->GetDef() || Self->IsUnconscious())
	{
		if (Self && SpecialPhase != 0)
		{
			EndSpecial(Self);
		}
		return;
	}
	if (Self->IsTamed())
	{
		TickTame(Self, DeltaSeconds);
	}
	else
	{
		TickWild(Self, DeltaSeconds);
	}
}

namespace
{
	void CreatureHit(AWRCreature* Self, AWRCharacterBase* Victim, FName Kind);
}

void AWRCreatureAI::ChaseAndBite(AWRCreature* Self, AWRCharacterBase* Target)
{
	const FWRCreatureDef* Def = Self->GetDef();
	const FVector To = Target->GetActorLocation() - Self->GetActorLocation();
	const float Dist = To.Size2D();
	const float Range = Def->AttackRange + Target->GetSimpleCollisionRadius();
	if (Self->HasTag(WRTags::State_Attacking))
	{
		// Mid-bite: feet planted for the wind-up (its telegraph), turning toward the target only through the first part
		// of it, so a late step aside or back still makes it miss. Nothing else starts until the bite is done.
		if (GetWorld()->GetTimeSeconds() < BiteTrackUntil && !To.IsNearlyZero())
		{
			Self->SetActorRotation(FMath::RInterpConstantTo(Self->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f),
				GetWorld()->GetDeltaSeconds(), WRTune(TEXT("creature.biteTrackRate"), 180.f)));
		}
		return;
	}
	if (SpecialPhase != 0 || TryStartSpecial(Self, Target, Dist))
	{
		return;  // TickSpecial runs it
	}
	if (Dist > Range)
	{
		FVector Dir = To.GetSafeNormal2D();
		if (Def->Archetype == TEXT("pack_hunter") && Dist > 500.f && Self->HuntTarget.Get() != Target)
		{
			// Pack members swing wide to come at the target from different angles (not at running prey: they'd never catch it).
			Dir = Dir.RotateAngleAxis(FlankAngle * FMath::Clamp((Dist - 500.f) / 800.f, 0.f, 1.f), FVector::UpVector);
		}
		if (Def->Archetype == TEXT("flyer") && !Self->HasTag(WRTags::Status_Staggered))
		{
			// Flyers circle high, then dive when close.
			const float WantZ = Target->GetActorLocation().Z + (Dist > 700.f ? 450.f : 0.f);
			Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Dir.Z = FMath::Clamp((WantZ - Self->GetActorLocation().Z) / 300.f, -1.f, 1.f);
		}
		Self->AddMovementInput(Dir, 1.f);
		return;
	}
	SetControlRotation(To.Rotation());
	if (Self->HuntTarget.Get() == Target)
	{
		// Prey can't block or dodge: a quick snap in reach (a swept bite would miss something running), keeping pace.
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextSnapAt)
		{
			NextSnapAt = Now + FWRTuning::Get().Num(TEXT("creatureAI.bite.cooldown"), 1.2f);
			Self->SetActorRotation(To.GetSafeNormal2D().Rotation());
			Self->PlaySwing(EWRSwingKind::Snap, 0.4f);
			CreatureHit(Self, Target, TEXT("bite"));
		}
		Self->AddMovementInput(To.GetSafeNormal2D(), 1.f);
		return;
	}
	if (UAbilitySystemComponent* ASC = Self->GetAbilitySystemComponent(); ASC && ASC->TryActivateAbilityByClass(UWRGA_Bite::StaticClass()))
	{
		const float Windup = WRTune(TEXT("creature.biteTiming.windup"), 0.5f);
		BiteTrackUntil = GetWorld()->GetTimeSeconds() + Windup * WRTune(TEXT("creature.biteTrackFraction"), 0.5f);
		UE_LOG(LogWildreach, Verbose, TEXT("AI %s winds up bite at %s (%.2f s)"), *Self->GetName(), *Target->GetName(), Windup);
	}
}

AWRCreature* AWRCreatureAI::PickPrey(AWRCreature* Self) const
{
	const FWRCreatureDef* Def = Self->GetDef();
	if (!Def || Def->Diet != TEXT("carnivore"))
	{
		return nullptr;
	}
	AWRCreature* Best = nullptr;
	float BestDist = Def->AggroRadius;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		const FWRCreatureDef* PreyDef = It->GetDef();
		if (*It == Self || !PreyDef || !It->IsAlive() || It->IsTamed() || It->IsUnconscious() || It->IsHidden() || !FWRCreatureDef::CanHunt(*Def, *PreyDef))
		{
			continue;
		}
		const float Dist = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void AWRCreatureAI::UpdateSleep(AWRCreature* Self)
{
	const FWRCreatureDef* Def = Self->GetDef();
	const bool bNight = AWRGameState::IsNightIn(Self);
	const bool bBedtime = (Def->Activity == TEXT("diurnal") && bNight) || (Def->Activity == TEXT("nocturnal") && !bNight);
	const bool bBusy = Self->GetAggroTarget() || Self->IsFleeing() || Self->IsEating() || Self->HuntTarget.IsValid() || Self->WasWokenRecently();
	Self->SetAsleep(bBedtime && !bBusy);
}

namespace
{
	FString AttackKey(FName Kind, const TCHAR* Field) { return FString::Printf(TEXT("creatureAI.%s.%s"), *Kind.ToString(), Field); }
	float AttackNum(FName Kind, const TCHAR* Field, float Fallback) { return FWRTuning::Get().Num(AttackKey(Kind, Field), Fallback); }

	void CreatureHit(AWRCreature* Self, AWRCharacterBase* Victim, FName Kind)
	{
		FWRHit Hit;
		Hit.Instigator = Self;
		Hit.Damage = Self->Weapon.FlatDamage * AttackNum(Kind, TEXT("damageMult"), 1.3f);
		Hit.PoiseDamage = AttackNum(Kind, TEXT("poise"), 25.f);
		Hit.Element = WRTags::Element_Physical;
		Hit.Origin = Self->GetActorLocation();
		Hit.bMelee = true;
		Hit.bHeavy = true;
		const EWRHitResult Result = UWRCombatLibrary::ApplyHit(Victim, Hit);
		UE_LOG(LogWildreach, Log, TEXT("AI %s %s hits %s"), *Self->GetName(), *Kind.ToString(), *Victim->GetName());
		const float Knockback = AttackNum(Kind, TEXT("knockback"), 0.f);
		if (Knockback > 0.f && Result != EWRHitResult::Dodged && Result != EWRHitResult::Parried)
		{
			const FVector Push = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, Knockback * 0.35f);
			Victim->LaunchCharacter(Push, true, true);
		}
	}
}

bool AWRCreatureAI::TryStartSpecial(AWRCreature* Self, AWRCharacterBase* Target, float Dist)
{
	const FWRCreatureDef* Def = Self->GetDef();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Def || Def->Attacks.Num() == 0 || Self->HasTag(WRTags::Status_Staggered) || Self->GetCharacterMovement()->IsFalling())
	{
		return false;
	}
	for (const FName& Kind : Def->Attacks)
	{
		if (Now < SpecialReadyAt.FindRef(Kind) || ((Kind == TEXT("lunge") || Kind == TEXT("charge")) && Self->GetSpeedMultiplier() <= 0.f))
		{
			continue;  // cooling down, or rooted / frozen: no leaping or charging (it can still spit or stomp)
		}
		const float Min = Kind == TEXT("stomp") ? 0.f : AttackNum(Kind, TEXT("min"), 300.f);
		const float Max = Kind == TEXT("stomp") ? AttackNum(Kind, TEXT("radius"), 320.f) + Target->GetSimpleCollisionRadius() - 20.f : AttackNum(Kind, TEXT("max"), 800.f);
		if (Dist < Min || Dist > Max)
		{
			continue;
		}
		const float Windup = AttackNum(Kind, TEXT("windup"), 0.5f);
		Special = Kind;
		SpecialPhase = 1;
		SpecialClock = Windup;
		SpecialTarget = Target;
		bSpecialHit = false;
		SpecialDir = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		SpecialReadyAt.Add(Kind, Now + Windup + AttackNum(Kind, TEXT("cooldown"), 6.f));
		// Its attack clip, timed so the strike lands with the move (and paced to read differently for each).
		const EWRSwingKind Swing = Kind == TEXT("lunge") ? EWRSwingKind::Lunge : Kind == TEXT("charge") ? EWRSwingKind::Charge
			: Kind == TEXT("stomp") ? EWRSwingKind::Stomp : Kind == TEXT("spit") ? EWRSwingKind::Spit : EWRSwingKind::Bite;
		Self->PlaySwing(Swing, Windup);
		// Telegraphs: where the charge will run, the ring a stomp hits.
		const FVector Floor = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() - 3.f);
		if (Kind == TEXT("charge"))
		{
			const float Length = Self->GetCharacterMovement()->GetMaxSpeed() * AttackNum(Kind, TEXT("speedMult"), 2.f) * AttackNum(Kind, TEXT("seconds"), 1.2f);
			AWRGroundMarker::Spawn(GetWorld(), Floor, SpecialDir.Rotation(), FVector2D(Length, Self->GetSimpleCollisionRadius()), true, FLinearColor(1.f, 0.35f, 0.1f), Windup);
		}
		else if (Kind == TEXT("stomp"))
		{
			const float Radius = AttackNum(Kind, TEXT("radius"), 320.f);
			AWRGroundMarker::Spawn(GetWorld(), Floor, FRotator::ZeroRotator, FVector2D(Radius, Radius), false, FLinearColor(1.f, 0.2f, 0.05f), Windup);
		}
		UE_LOG(LogWildreach, Log, TEXT("AI %s winds up %s at %s"), *Self->GetName(), *Kind.ToString(), *Target->GetName());
		return true;
	}
	return false;
}

void AWRCreatureAI::TickSpecial(AWRCreature* Self, float DeltaSeconds)
{
	AWRCharacterBase* Target = SpecialTarget.Get();
	SpecialClock -= DeltaSeconds;
	if (SpecialPhase == 1)
	{
		// Windup: it keeps facing the target (a charge commits to its line when it starts).
		if (Target && Special != TEXT("charge"))
		{
			SpecialDir = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		}
		if (!SpecialDir.IsNearlyZero())
		{
			Self->SetActorRotation(SpecialDir.Rotation());
		}
		if (SpecialClock > 0.f)
		{
			return;
		}
		SpecialPhase = 2;
		UE_LOG(LogWildreach, Log, TEXT("AI %s uses %s"), *Self->GetName(), *Special.ToString());
		if (Special == TEXT("lunge"))
		{
			// A leap at where the target will be when it lands (running prey included).
			const float Flight = 0.45f;
			const FVector Aim = Target ? Target->GetActorLocation() + Target->GetVelocity() * FVector(1.f, 1.f, 0.f) * Flight : Self->GetActorLocation() + SpecialDir * 400.f;
			const FVector Leap = (Aim - Self->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			const float Speed = AttackNum(Special, TEXT("speed"), 1100.f);
			SpecialDir = Leap.GetSafeNormal();
			Self->SetActorRotation(SpecialDir.Rotation());
			Self->LaunchCharacter(SpecialDir * FMath::Min(Speed, Leap.Size() / Flight) + FVector(0.f, 0.f, AttackNum(Special, TEXT("lift"), 380.f)), true, true);
			SpecialClock = 0.9f;
		}
		else if (Special == TEXT("charge"))
		{
			Self->SpeedBoost = AttackNum(Special, TEXT("speedMult"), 2.f);
			SpecialClock = AttackNum(Special, TEXT("seconds"), 1.2f);
		}
		else if (Special == TEXT("spit"))
		{
			// A glob of its element (fire / frost) at the target.
			const FWRCreatureDef* Def = Self->GetDef();
			const bool bFrost = Def && Def->Element == TEXT("frost");
			const FVector From = Self->GetActorLocation() + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() * 0.5f) + SpecialDir * Self->GetSimpleCollisionRadius();
			const FVector Aim = Target ? (Target->GetActorLocation() - From).GetSafeNormal() : SpecialDir;
			FActorSpawnParameters Params;
			Params.Owner = Self;
			Params.Instigator = Self;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRProjectile* Glob = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Aim.Rotation(), Params))
			{
				FWRHit Hit;
				Hit.Instigator = Self;
				Hit.Damage = Self->Weapon.FlatDamage * AttackNum(Special, TEXT("damageMult"), 0.8f);
				Hit.PoiseDamage = AttackNum(Special, TEXT("poise"), 10.f);
				Hit.Element = bFrost ? WRTags::Element_Frost : WRTags::Element_Fire;
				(bFrost ? Hit.FrostBuildup : Hit.BurnBuildup) = AttackNum(Special, TEXT("buildup"), 25.f);
				Glob->Init(Hit, AttackNum(Special, TEXT("speed"), 1800.f), bFrost ? FLinearColor(0.5f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 0.05f));
			}
			EndSpecial(Self);
		}
		else if (Special == TEXT("stomp"))
		{
			// Everything hostile in the ring takes it (not a knocked-out creature: someone may be taming it).
			const float Radius = AttackNum(Special, TEXT("radius"), 320.f);
			WRFx::Broadcast(Self, TEXT("stomp"), Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, Radius / 100.f, TEXT("stomp"));
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && (!It->HasTag(WRTags::Status_Unconscious) || *It == SpecialTarget.Get()) && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
				{
					CreatureHit(Self, *It, Special);
				}
			}
			EndSpecial(Self);
		}
		else
		{
			EndSpecial(Self);
		}
		return;
	}
	// Under way: a charge runs its line; both it and a lunge hit the first hostile they reach (passing over knocked-out ones).
	if (Special == TEXT("charge"))
	{
		Self->AddMovementInput(SpecialDir, 1.f);
	}
	if (!bSpecialHit)
	{
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			const float Reach = Self->GetSimpleCollisionRadius() + It->GetSimpleCollisionRadius() + AttackNum(Special, TEXT("reach"), 60.f);
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && (!It->HasTag(WRTags::Status_Unconscious) || *It == SpecialTarget.Get()) && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Reach)
			{
				bSpecialHit = true;
				CreatureHit(Self, *It, Special);
				break;
			}
		}
	}
	const bool bLanded = Special == TEXT("lunge") && SpecialClock < 0.6f && !Self->GetCharacterMovement()->IsFalling();
	if (SpecialClock <= 0.f || bLanded || (Special == TEXT("charge") && bSpecialHit))
	{
		EndSpecial(Self);
	}
}

void AWRCreatureAI::EndSpecial(AWRCreature* Self)
{
	Self->SpeedBoost = 1.f;
	SpecialPhase = 0;
	Special = NAME_None;
	SpecialTarget = nullptr;
}

void AWRCreatureAI::TickWild(AWRCreature* Self, float DeltaSeconds)
{
	const FWRCreatureDef* Def = Self->GetDef();
	if (SpecialPhase != 0)
	{
		TickSpecial(Self, DeltaSeconds);
		return;
	}

	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.25f;
		UpdateSleep(Self);
		// Morale: badly hurt, or hurt with the pack falling around it, it runs (bosses and minions never do).
		const UWRAttributeSet* A = Self->GetAttributes();
		const float Health = A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
		if (!Self->IsFleeing() && Self->SinceHit() < 10.f && Def->Behavior != TEXT("passive"))
		{
			AActor* Threat = Self->GetAggroTarget() ? Self->GetAggroTarget() : Self->GetFleeFrom();
			if (Health < WRTune(TEXT("creatureAI.fleeHealth"), 0.25f))
			{
				Self->StartFleeing(Threat, TEXT("badly hurt"));
			}
			else if (Self->RecentPackLoss() && Health < WRTune(TEXT("creatureAI.packFleeHealth"), 0.5f))
			{
				Self->StartFleeing(Threat, TEXT("the pack is breaking"));
			}
		}
		const AWRCharacterBase* Current = Cast<AWRCharacterBase>(Self->GetAggroTarget());
		if (Self->IsFleeing() || Self->IsEating())
		{
			// Running or feeding: no new fights.
		}
		else if (!Current || !Current->IsAlive() || (Current->IsCreature() && Current->GetTameOwner().IsEmpty()))
		{
			// A survivor or a tame in range comes first (a hunter drops its prey for one); then, if hungry, prey.
			AActor* Picked = PickTarget(Self);
			if (Picked)
			{
				Self->HuntTarget = nullptr;
				Self->SetAggroTarget(Picked);
			}
			else if (!Current || !Current->IsAlive())
			{
				AWRCreature* Prey = Self->IsHungry() && !Self->IsAsleep() ? PickPrey(Self) : nullptr;
				if (Prey)
				{
					UE_LOG(LogWildreach, Log, TEXT("AI %s hunts %s"), *Self->GetName(), *Prey->GetName());
				}
				Self->HuntTarget = Prey;
				Self->SetAggroTarget(Prey);
			}
		}
	}

	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	if (Target && !Target->IsAlive())
	{
		Self->SetAggroTarget(nullptr);
		Target = nullptr;
	}
	const bool bOnHunt = Target && Self->HuntTarget.Get() == Target;
	const float Leash = bOnHunt ? WRTune(TEXT("creatureAI.huntLeash"), 8000.f) : WRTune(TEXT("creature.leashDistance"), 4000.f);
	if (Target && FVector::Dist(Self->GetActorLocation(), Self->GetHome()) > Leash)
	{
		// Gave up the chase (a hunt runs further): head home.
		if (Self->HuntTarget.IsValid())
		{
			Self->HuntFailed();
		}
		Self->SetAggroTarget(nullptr);
		Target = nullptr;
	}

	if (Self->HuntTarget.IsValid() && GetWorld()->GetTimeSeconds() >= NextChaseLogAt)
	{
		NextChaseLogAt = GetWorld()->GetTimeSeconds() + 3.0;
		UE_LOG(LogWildreach, Verbose, TEXT("AI %s chasing %s at %.0f cm (%.0f from home)"), *Self->GetName(), *GetNameSafe(Self->HuntTarget.Get()),
			FVector::Dist2D(Self->HuntTarget->GetActorLocation(), Self->GetActorLocation()), FVector::Dist2D(Self->GetActorLocation(), Self->GetHome()));
	}
	// Running for its life: away from the threat, a little faster than usual.
	if (Self->IsFleeing())
	{
		Self->SpeedBoost = WRTune(TEXT("creatureAI.fleeSpeedMult"), 1.2f);
		const AActor* From = Self->GetFleeFrom();
		const FVector Away = From ? Self->GetActorLocation() - From->GetActorLocation() : Self->GetActorLocation() - Self->GetHome();
		Self->AddMovementInput(Away.IsNearlyZero() ? Self->GetActorForwardVector() : Away.GetSafeNormal2D(), 1.f);
		return;
	}
	// In pursuit of prey it runs flat out (slow prey gets caught, fast prey can still get away).
	Self->SpeedBoost = Self->HuntTarget.IsValid() && Target == Self->HuntTarget.Get() ? WRTune(TEXT("creatureAI.huntSpeedMult"), 1.3f) : 1.f;
	if (Self->IsEating())
	{
		return;  // over its kill
	}

	// Burrowers stay underground until they have a target.
	if (Def->Archetype == TEXT("burrower"))
	{
		const bool bWantHidden = Target == nullptr && WRTune(TEXT("creature.burrowHide"), 1.f) > 0.f;  // 0 = always visible (art checks)
		if (Self->IsHidden() != bWantHidden)
		{
			Self->SetActorHiddenInGame(bWantHidden);
			Self->SetActorEnableCollision(!bWantHidden);
			// Without collision nothing holds it up: buried, it waits where it is instead of falling out of the world.
			if (bWantHidden) Self->GetCharacterMovement()->DisableMovement();
			else Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	// Passive creatures flee from nearby survivors (and from anyone who hurt them), and from a predator hunting them.
	if (Def->Behavior == TEXT("passive"))
	{
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			const FVector Away = Self->GetActorLocation() - It->GetActorLocation();
			if (It->IsAlive() && Away.Size2D() < 600.f && It->GetVelocity().Size2D() > 250.f)
			{
				Self->AddMovementInput(Away.GetSafeNormal2D(), 1.f);
				return;
			}
		}
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			// It bolts in bursts: after a run it needs creatureAI.preyRestSeconds before it can bolt again (a bite still sends it).
			if (It->HuntTarget.Get() == Self && It->IsAlive() && Self->CanBoltAgain()
				&& FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < WRTune(TEXT("creatureAI.preyNoticeRadius"), 1500.f))
			{
				Self->StartFleeing(*It, TEXT("hunted"));
				return;
			}
		}
	}

	if (Target)
	{
		ChaseAndBite(Self, Target);
		return;
	}

	// Idle: drift around home (or lie still, asleep).
	if (Def->Archetype == TEXT("flyer"))
	{
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	if (Self->IsAsleep())
	{
		return;
	}
	WanderClock -= DeltaSeconds;
	if (WanderClock <= 0.f)
	{
		WanderClock = FMath::FRandRange(4.f, 9.f);
		WanderGoal = Self->GetHome() + FVector(FMath::RandPointInCircle(Def->Archetype == TEXT("herd") ? 900.f : 400.f), 0.f);
	}
	const FVector ToGoal = WanderGoal - Self->GetActorLocation();
	if (ToGoal.Size2D() > 150.f)
	{
		Self->AddMovementInput(ToGoal.GetSafeNormal2D(), 0.4f);
	}
}

void AWRCreatureAI::TickTame(AWRCreature* Self, float DeltaSeconds)
{
	Self->SpeedBoost = 1.f;
	Self->SetAsleep(false);
	if (SpecialPhase != 0)
	{
		TickSpecial(Self, DeltaSeconds);
		return;
	}
	// Find the owner's body.
	AWRCharacter* OwnerBody = nullptr;
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerState* PS = It->GetPlayerState();
		if (PS && PS->GetPlayerName() == Self->TamedBy) { OwnerBody = *It; break; }
	}
	if (Self->IsHidden())
	{
		Self->SetActorHiddenInGame(false);
		Self->SetActorEnableCollision(true);
	}

	// Fight the owner's target (or whatever attacks us) when aggressive.
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	if (Self->bTameAggressive && OwnerBody && OwnerBody->CombatTarget.IsValid() && OwnerBody->CombatTarget->IsAlive() && Self->IsHostileTo(OwnerBody->CombatTarget.Get()))
	{
		Target = OwnerBody->CombatTarget.Get();
	}
	// Not a knocked-out creature (no threat, and likely the owner's next tame), and a tame told to stay guards its post.
	if (Target && (!Target->IsAlive() || !Self->bTameAggressive || Target->HasTag(WRTags::Status_Unconscious)
		|| (OwnerBody && FVector::Dist(Target->GetActorLocation(), OwnerBody->GetActorLocation()) > 2500.f)
		|| (Self->TameOrder == EWRTameOrder::Stay && FVector::Dist(Target->GetActorLocation(), Self->GetHome()) > WRTune(TEXT("taming.stayGuardRadius"), 1500.f))))
	{
		Target = nullptr;
	}
	Self->SetAggroTarget(Target);
	if (Target)
	{
		ChaseAndBite(Self, Target);
		return;
	}

	const FVector Goal = Self->TameOrder == EWRTameOrder::Follow && OwnerBody ? OwnerBody->GetActorLocation() : Self->GetHome();
	const FVector To = Goal - Self->GetActorLocation();
	const float Stop = Self->TameOrder == EWRTameOrder::Follow ? 350.f : 100.f;
	if (To.Size2D() > Stop)
	{
		Self->AddMovementInput(To.GetSafeNormal2D(), To.Size2D() > 1200.f ? 1.f : 0.7f);
	}
}

// ---------------------------------------------------------------- spawner & nests

AWRCreatureSpawner::AWRCreatureSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCreatureSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	if (Def && Def->Archetype == TEXT("boss"))
	{
		RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
	}
	if (ActivationRadius <= 0.f)
	{
		Activate();
	}
	if (Def && Def->bNest)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRNest* Nest = GetWorld()->SpawnActor<AWRNest>(AWRNest::StaticClass(), GroundAt(GetActorLocation() + FVector(150.f, 0.f, 0.f)), FRotator::ZeroRotator, Params))
		{
			Nest->CreatureId = CreatureId;
			Nest->Spawner = this;
		}
	}
}

int32 AWRCreatureSpawner::SpawnSitesFromData(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || WRTune(TEXT("wildlife.enabled"), 1.f) <= 0.f)
	{
		return 0;
	}
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("island_spawns.json"));
	const TArray<TSharedPtr<FJsonValue>>* Sites = nullptr;
	FString Map;
	if (!Root || !Root->TryGetStringField(TEXT("map"), Map) || Map != UWorld::RemovePIEPrefix(World->GetMapName()) || !Root->TryGetArrayField(TEXT("sites"), Sites))
	{
		return 0;
	}
	const float Activation = WRTune(TEXT("wildlife.activationRadius"), 25000.f);
	int32 Spawned = 0, Creatures = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Sites)
	{
		const TSharedPtr<FJsonObject> Site = Value.IsValid() ? Value->AsObject() : nullptr;
		const FName Id = Site ? FName(*Site->GetStringField(TEXT("creature"))) : NAME_None;
		if (!FWRCreatureRegistry::Get().Find(Id))
		{
			UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE site for unknown creature %s"), *Id.ToString());
			continue;
		}
		const FTransform At(FVector(Site->GetNumberField(TEXT("x")), Site->GetNumberField(TEXT("y")), Site->GetNumberField(TEXT("z")) + 100.f));
		if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Spawner->CreatureId = Id;
			Spawner->Count = FMath::Max(1, Site->GetIntegerField(TEXT("count")));
			Spawner->Radius = Site->GetNumberField(TEXT("radius"));
			Spawner->RespawnSeconds = Site->GetNumberField(TEXT("respawn"));
			Spawner->ActivationRadius = Activation;
			Spawner->bTraceGround = true;
			Spawner->FinishSpawning(At);
			++Spawned;
			Creatures += Spawner->Count;
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("WILDLIFE %d sites, %d creatures (Data/island_spawns.json); a pack comes out when a player is within %.0f m"), Spawned, Creatures, Activation / 100.f);
	return Spawned;
}

int32 AWRCreatureSpawner::NumAlive() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AWRCreature>& C : Alive)
	{
		N += C.IsValid() && C->IsAlive() && !C->IsTamed();
	}
	return N;
}

void AWRCreatureSpawner::Activate()
{
	bActive = true;
	AloneSince = -1.0;
	// Top the pack up; a death still waiting out its respawn keeps its place.
	const double Now = GetWorld()->GetTimeSeconds();
	PendingRespawns.RemoveAll([Now](double At) { return At <= Now; });
	for (int32 i = NumAlive() + PendingRespawns.Num(); i < Count; ++i)
	{
		SpawnOne();
	}
	if (ActivationRadius > 0.f)
	{
		UE_LOG(LogWildreach, Log, TEXT("WILDLIFE %s out at %s (%d)"), *CreatureId.ToString(), *GetActorLocation().ToCompactString(), NumAlive());
	}
}

void AWRCreatureSpawner::Deactivate()
{
	bActive = false;
	AloneSince = -1.0;
	int32 Gone = 0;
	for (const TWeakObjectPtr<AWRCreature>& C : Alive)
	{
		// One knocked out mid-tame stays where its tamer left it.
		if (C.IsValid() && C->IsAlive() && !C->IsTamed() && !C->IsUnconscious())
		{
			C->Destroy();
			++Gone;
		}
	}
	Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
	UE_LOG(LogWildreach, Log, TEXT("WILDLIFE %s at %s went quiet (%d gone)"), *CreatureId.ToString(), *GetActorLocation().ToCompactString(), Gone);
}

bool AWRCreatureSpawner::PlayerNear(float Range) const
{
	// Near the site or near any of the pack (a hunt can carry it a long way from home).
	const float RangeSq = FMath::Square(Range);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const FVector At = Pawn->GetActorLocation();
		if (FVector::DistSquared2D(At, GetActorLocation()) < RangeSq)
		{
			return true;
		}
		for (const TWeakObjectPtr<AWRCreature>& C : Alive)
		{
			if (C.IsValid() && FVector::DistSquared2D(At, C->GetActorLocation()) < RangeSq)
			{
				return true;
			}
		}
	}
	return false;
}

FVector AWRCreatureSpawner::GroundAt(const FVector& Point) const
{
	if (!bTraceGround)
	{
		return Point;
	}
	// The terrain under the point, not a tree crown or a roof over it.
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, Point + FVector(0.f, 0.f, 3000.f), Point - FVector(0.f, 0.f, 6000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(WRSpawnGround), false, this));
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
		{
			return Hit.ImpactPoint;
		}
	}
	return Hits.Num() > 0 ? Hits.Last().ImpactPoint : Point;
}

void AWRCreatureSpawner::SpawnOne()
{
	const FVector2D Offset = FMath::RandPointInCircle(Radius);
	if (AWRCreature* Creature = AWRCreature::SpawnCreature(GetWorld(), CreatureId, GroundAt(GetActorLocation() + FVector(Offset, 0.f)), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
	{
		Creature->Spawner = this;
		Alive.Add(Creature);
		Creature->OnCreatureDied.AddWeakLambda(this, [this](AWRCreature*)
		{
			PendingRespawns.Add(GetWorld()->GetTimeSeconds() + RespawnSeconds);
		});
	}
}

void AWRCreatureSpawner::AlertPack(AActor* Attacker)
{
	for (const TWeakObjectPtr<AWRCreature>& C : Alive)
	{
		if (C.IsValid() && !C->IsTamed() && !C->GetAggroTarget())
		{
			C->SetAggroTarget(Attacker);
		}
	}
}

void AWRCreatureSpawner::NotifyPackLoss(const AWRCreature* Dead)
{
	for (const TWeakObjectPtr<AWRCreature>& C : Alive)
	{
		if (C.IsValid() && C.Get() != Dead && C->IsAlive() && !C->IsTamed())
		{
			C->NotePackLoss();
		}
	}
}

void AWRCreatureSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || C->IsTamed(); });
	const double Now = GetWorld()->GetTimeSeconds();
	if (ActivationRadius > 0.f)
	{
		// A little further to leave than to arrive, so a player on the edge doesn't flicker the pack in and out.
		const bool bNear = PlayerNear(bActive ? ActivationRadius * WRTune(TEXT("wildlife.leaveRadiusMult"), 1.2f) : ActivationRadius);
		if (!bActive)
		{
			if (bNear)
			{
				Activate();
			}
			return;
		}
		if (bNear)
		{
			AloneSince = -1.0;
		}
		else if (AloneSince < 0.0)
		{
			AloneSince = Now;
		}
		else if (Now - AloneSince >= WRTune(TEXT("wildlife.despawnSeconds"), 90.f))
		{
			Deactivate();
			return;
		}
	}
	for (int32 i = PendingRespawns.Num() - 1; i >= 0; --i)
	{
		if (PendingRespawns[i] <= Now)
		{
			PendingRespawns.RemoveAt(i);
			SpawnOne();
		}
	}
}

AWRNest::AWRNest()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
	Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.4f));
}

void AWRNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRNest, CreatureId);
	DOREPLIFETIME(AWRNest, bHasEgg);
}

FString AWRNest::TakeEgg(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority())
	{
		return FString();
	}
	if (!bHasEgg)
	{
		return TEXT("The nest is empty (eggs come back in a few minutes)");
	}
	const FName Egg(*FString::Printf(TEXT("egg_%s"), *CreatureId.ToString()));
	if (PS->GetInventory()->AddItem(Egg, 1) > 0)
	{
		return TEXT("Inventory full");
	}
	bHasEgg = false;
	GetWorldTimerManager().SetTimer(RegrowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { bHasEgg = true; }), WRTune(TEXT("taming.eggRegrowSeconds"), 300.f), false);
	// The pack swarms whoever robbed the nest.
	if (AWRCreatureSpawner* S = Spawner.Get())
	{
		S->AlertPack(Player->GetPawn());
	}
	return TEXT("Took an egg! Use it to hatch a tame. The nest's guardians are angry...");
}
