#include "WRCreature.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------- registry

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
		if (!O.IsValid())
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

		const TArray<TSharedPtr<FJsonValue>>* Color = nullptr;
		if (O->TryGetArrayField(TEXT("color"), Color) && Color->Num() >= 3)
		{
			D.Color = FLinearColor((*Color)[0]->AsNumber(), (*Color)[1]->AsNumber(), (*Color)[2]->AsNumber());
		}
		const TSharedPtr<FJsonObject>* Loot = nullptr;
		if (O->TryGetObjectField(TEXT("loot"), Loot))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*Loot)->Values)
			{
				D.Loot.Add({ FName(*L.Key), static_cast<int32>(L.Value->AsNumber()) });
			}
		}
		else if (D.Archetype != TEXT("dummy"))
		{
			D.Loot = { { TEXT("raw_meat"), 2 }, { TEXT("hide"), 1 } };
		}
		const TArray<TSharedPtr<FJsonValue>>* Resists = nullptr;
		if (O->TryGetArrayField(TEXT("resists"), Resists))
		{
			for (const TSharedPtr<FJsonValue>& R : *Resists)
			{
				D.Resists.Add(FName(*R->AsString()));
			}
		}
		Defs.Add(D);
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d creature definitions"), Defs.Num());
}

// ---------------------------------------------------------------- creature

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
}

AWRCreature* AWRCreature::SpawnCreature(UWorld* World, FName CreatureId, const FVector& Location, const FRotator& Rotation)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	if (!World || !Def)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Unknown creature %s"), *CreatureId.ToString());
		return nullptr;
	}
	UClass* Class = Def->Archetype == TEXT("dummy") ? AWRTrainingDummy::StaticClass() : AWRCreature::StaticClass();
	const FTransform Transform(Rotation, Location + FVector(0.f, 0.f, Def->HalfHeight + 5.f));
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
	BaseBodyColor = Def->Color;
	SetBodyColor(Def->Color);
}

void AWRCreature::OnRep_CreatureId()
{
	ApplyDef();
}

void AWRCreature::BeginPlay()
{
	ApplyDef();
	Super::BeginPlay();
	Home = GetActorLocation();
	AbilitySystem->InitAbilityActorInfo(this, this);

	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def)
	{
		return;
	}
	InitVitals(Def->Health, 100.f, Def->Poise);
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

void AWRCreature::NotifyAttackedBy(AActor* Attacker)
{
	// Anything you hit fights back (neutral herds and passives included, for now).
	if (!AggroTarget.IsValid())
	{
		AggroTarget = Attacker;
	}
}

FString AWRCreature::GetDisplayLabel() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? Def->Name : GetName();
}

void AWRCreature::OnDeath(AActor* Killer)
{
	OnCreatureDied.Broadcast(this);
	if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Killer))
	{
		if (const FWRCreatureDef* Def = GetDef())
		{
			for (const TPair<FName, int32>& L : Def->Loot)
			{
				Inv->AddItem(L.Key, L.Value);
			}
		}
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
	float BestDist = Def->AggroRadius;
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive())
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

void AWRCreatureAI::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AWRCreature* Self = Cast<AWRCreature>(GetPawn());
	if (!Self || !Self->IsAlive() || !Self->GetDef())
	{
		return;
	}
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
	const float Leash = WRTune(TEXT("creature.leashDistance"), 4000.f);
	if (Target && FVector::Dist(Self->GetActorLocation(), Self->GetHome()) > Leash)
	{
		// Gave up the chase: head home.
		Self->SetAggroTarget(nullptr);
		Target = nullptr;
	}

	if (!Target)
	{
		const FVector ToHome = Self->GetHome() - Self->GetActorLocation();
		if (ToHome.Size2D() > 150.f)
		{
			Self->AddMovementInput(ToHome.GetSafeNormal2D(), 0.6f);
		}
		return;
	}

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
		Self->AddMovementInput(Dir, 1.f);
		return;
	}

	// In range: face the target and bite (the ability's own cooldown paces it).
	SetControlRotation(To.Rotation());
	if (UAbilitySystemComponent* ASC = Self->GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UWRGA_Bite::StaticClass());
	}
}

// ---------------------------------------------------------------- spawner

AWRCreatureSpawner::AWRCreatureSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCreatureSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		for (int32 i = 0; i < Count; ++i)
		{
			SpawnOne();
		}
	}
}

void AWRCreatureSpawner::SpawnOne()
{
	const FVector2D Offset = FMath::RandPointInCircle(Radius);
	if (AWRCreature* Creature = AWRCreature::SpawnCreature(GetWorld(), CreatureId, GetActorLocation() + FVector(Offset, 0.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
	{
		Alive.Add(Creature);
		Creature->OnCreatureDied.AddWeakLambda(this, [this](AWRCreature*)
		{
			PendingRespawns.Add(GetWorld()->GetTimeSeconds() + RespawnSeconds);
		});
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
	Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
}
