#include "WRCreature.h"
#include "WRBoss.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "GameFramework/GameStateBase.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- registry

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
		const TSharedPtr<FJsonObject>* AnimObj = nullptr;
		if (O->TryGetObjectField(TEXT("anims"), AnimObj))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& A : (*AnimObj)->Values) D.Anims.Add(FName(*A.Key), A.Value->AsString());
		}
		D.TamingMethods = ParseNames(O, TEXT("taming"));
		D.Food = ParseNames(O, TEXT("food"));
		D.Products = ParseCounts(O, TEXT("products"));
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

	// Real art: swap the placeholder shape for the skeletal mesh, fitted to the capsule with its feet on the ground.
	USkeletalMesh* ArtMesh = Def->Mesh.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *Def->Mesh);
	if (ArtMesh && GetNetMode() != NM_DedicatedServer)
	{
		Skel->SetSkeletalMesh(ArtMesh);
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
		CurrentAnim = NAME_None;
		PlayCreatureAnim(TEXT("idle"), true);
	}
}

void AWRCreature::PlayCreatureAnim(FName Key, bool bLoop)
{
	TObjectPtr<UAnimSequenceBase>* Anim = AnimAssets.Find(Key);
	if (!Anim || !*Anim || (Key == CurrentAnim && bLoop))
	{
		return;
	}
	CurrentAnim = Key;
	Skel->PlayAnimation(*Anim, bLoop);
	if (!bLoop)
	{
		OneShotUntil = GetWorld()->GetTimeSeconds() + (*Anim)->GetPlayLength();
	}
}

void AWRCreature::TickCreatureAnim()
{
	if (!Skel->IsVisible() || AnimAssets.Num() == 0 || !IsAlive() || GetWorld()->GetTimeSeconds() < OneShotUntil)
	{
		return;
	}
	// Locomotion by speed (replicated movement on clients).
	const float Speed = GetVelocity().Size2D();
	const float Walk = GetCharacterMovement()->MaxWalkSpeed;
	PlayCreatureAnim(Speed > Walk * 0.65f ? FName(TEXT("run")) : Speed > 25.f ? FName(TEXT("walk")) : FName(TEXT("idle")), true);
}

void AWRCreature::OnSwingVisual(EWRSwingKind Kind, float Duration)
{
	if (Skel->IsVisible())
	{
		CurrentAnim = NAME_None;
		PlayCreatureAnim(TEXT("attack"), false);
	}
}

void AWRCreature::OnDeathVisual()
{
	if (Skel->IsVisible())
	{
		CurrentAnim = NAME_None;
		PlayCreatureAnim(TEXT("death"), false);
		OneShotUntil = 1.0e12;  // stay down
	}
}

void AWRCreature::OnRep_CreatureId()
{
	ApplyDef();
}

void AWRCreature::OnRep_Tamed()
{
	const FWRCreatureDef* Def = GetDef();
	GlowLight->SetVisibility(IsTamed() && Def && Def->Utility == TEXT("light"));
}

void AWRCreature::BeginPlay()
{
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
		// Wild: hostile to survivors and tames, never to other wild creatures.
		return !Other->IsCreature() || !OtherOwner.IsEmpty();
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
	return IsTamed() ? FString::Printf(TEXT("%s's %s"), *TamedBy, *Name) : Name;
}

void AWRCreature::NotifyAttackedBy(AActor* Attacker)
{
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
	// Damage interrupts taming.
	if (TameProgress > 0.f && IsUnconscious())
	{
		TameProgress *= WRTune(TEXT("taming.damageKeepsProgress"), 0.5f);
	}
	bCalm = false;
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
	FName FoodUsed;
	for (const FName& Food : Def->Food)
	{
		if (PS->GetInventory()->CountOf(Food) > 0) { FoodUsed = Food; break; }
	}
	if (FoodUsed.IsNone())
	{
		return FString::Printf(TEXT("%s eats %s"), *Def->Name, Def->Food.Num() > 0 ? *Def->Food[0].ToString() : TEXT("nothing you have"));
	}
	PS->GetInventory()->RemoveItem(FoodUsed, 1);
	TamerName = PS->GetPlayerName();
	TamerSpeed = 1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("tamingSpeed"));
	UWRLifeSkillsComponent::Award(PS, TEXT("taming"), TEXT("feed"), 5.f);
	bCalm = bOffer || bCalm;
	FedUntil = GetWorld()->GetTimeSeconds() + WRTune(TEXT("taming.secondsPerFood"), 30.f);
	return FString::Printf(TEXT("Fed %s: taming %.0f%% (needs %.0f s fed)"), *FoodUsed.ToString(), TameProgress * 100.f, Def->TameSeconds());
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
	OnRep_Tamed();
	UWRLifeSkillsComponent::Award(FindPlayerByName(GetWorld(), PlayerName), TEXT("taming"), TEXT("tame"), 100.f);
	if (Spawner.IsValid())
	{
		// No longer part of the wild pack; the spawner will replace it.
		OnCreatureDied.Broadcast(this);
		Spawner = nullptr;
	}
	UE_LOG(LogWildreach, Log, TEXT("TAME %s tamed by %s"), *GetName(), *PlayerName);
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
		TickCreatureAnim();
	}
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	const FWRCreatureDef* Def = GetDef();
	const float Now = GetWorld()->GetTimeSeconds();

	// Torpor wears off; an unconscious creature wakes when it reaches zero.
	if (Torpor > 0.f)
	{
		const float Rate = IsUnconscious() ? WRTune(TEXT("taming.unconsciousTorporDecay"), 0.01f) : WRTune(TEXT("taming.torporDecay"), 0.05f);
		Torpor = FMath::Max(0.f, Torpor - MaxTorpor * Rate * DeltaSeconds);
		if (Torpor <= 0.f && IsUnconscious())
		{
			Status->ClearStatus(WRTags::Status_Unconscious);
			bCalm = false;
			UE_LOG(LogWildreach, Log, TEXT("TAME %s woke up (%.0f%% tamed)"), *GetName(), TameProgress * 100.f);
		}
	}

	// Taming progresses while the creature is fed and either knocked out or calm.
	if (!IsTamed() && Def && !TamerName.IsEmpty() && Now < FedUntil && (IsUnconscious() || bCalm))
	{
		TameProgress = FMath::Min(1.f, TameProgress + DeltaSeconds * TamerSpeed / FMath::Max(1.f, Def->TameSeconds()));
		if (TameProgress >= 1.f)
		{
			CompleteTame(TamerName);
		}
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
		if (!It->IsAlive() || !Self->IsHostileTo(*It) || !It->IsHostileTo(Self))
		{
			continue;
		}
		if (FWRSocialRules::IsInSafeZone(GetWorld(), It->GetActorLocation()))
		{
			continue;
		}
		float Aggro = Def->AggroRadius;
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

void AWRCreatureAI::ChaseAndBite(AWRCreature* Self, AWRCharacterBase* Target)
{
	const FWRCreatureDef* Def = Self->GetDef();
	const FVector To = Target->GetActorLocation() - Self->GetActorLocation();
	const float Dist = To.Size2D();
	const float Range = Def->AttackRange + Target->GetSimpleCollisionRadius();
	if (Dist > Range)
	{
		FVector Dir = To.GetSafeNormal2D();
		if (Def->Archetype == TEXT("pack_hunter") && Dist > 500.f)
		{
			// Pack members swing wide to come at the target from different angles.
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
	if (UAbilitySystemComponent* ASC = Self->GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UWRGA_Bite::StaticClass());
	}
}

void AWRCreatureAI::TickWild(AWRCreature* Self, float DeltaSeconds)
{
	const FWRCreatureDef* Def = Self->GetDef();

	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.25f;
		const AWRCharacterBase* Current = Cast<AWRCharacterBase>(Self->GetAggroTarget());
		if (!Current || !Current->IsAlive())
		{
			Self->SetAggroTarget(PickTarget(Self));
		}
	}

	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	if (Target && FVector::Dist(Self->GetActorLocation(), Self->GetHome()) > WRTune(TEXT("creature.leashDistance"), 4000.f))
	{
		// Gave up the chase: head home.
		Self->SetAggroTarget(nullptr);
		Target = nullptr;
	}

	// Burrowers stay underground until they have a target.
	if (Def->Archetype == TEXT("burrower"))
	{
		const bool bWantHidden = Target == nullptr && WRTune(TEXT("creature.burrowHide"), 1.f) > 0.f;  // 0 = always visible (art checks)
		if (Self->IsHidden() != bWantHidden)
		{
			Self->SetActorHiddenInGame(bWantHidden);
			Self->SetActorEnableCollision(!bWantHidden);
		}
	}

	// Passive creatures flee from nearby survivors (and from anyone who hurt them).
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
	}

	if (Target)
	{
		ChaseAndBite(Self, Target);
		return;
	}

	// Idle: drift around home.
	if (Def->Archetype == TEXT("flyer"))
	{
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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
	if (Target && (!Target->IsAlive() || !Self->bTameAggressive || (OwnerBody && FVector::Dist(Target->GetActorLocation(), OwnerBody->GetActorLocation()) > 2500.f)))
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
	for (int32 i = 0; i < Count; ++i)
	{
		SpawnOne();
	}
	if (Def && Def->bNest)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRNest* Nest = GetWorld()->SpawnActor<AWRNest>(AWRNest::StaticClass(), GetActorLocation() + FVector(150.f, 0.f, 0.f), FRotator::ZeroRotator, Params))
		{
			Nest->CreatureId = CreatureId;
			Nest->Spawner = this;
		}
	}
}

void AWRCreatureSpawner::SpawnOne()
{
	const FVector2D Offset = FMath::RandPointInCircle(Radius);
	if (AWRCreature* Creature = AWRCreature::SpawnCreature(GetWorld(), CreatureId, GetActorLocation() + FVector(Offset, 0.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
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

void AWRCreatureSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = PendingRespawns.Num() - 1; i >= 0; --i)
	{
		if (PendingRespawns[i] <= Now)
		{
			PendingRespawns.RemoveAt(i);
			SpawnOne();
		}
	}
	Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || C->IsTamed(); });
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
