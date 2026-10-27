#include "WRBoss.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

static UStaticMeshComponent* MakeShapeMesh(AActor* Owner, const TCHAR* Name, const TCHAR* MeshPath)
{
	UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (Asset) Mesh->SetStaticMesh(Asset);
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		Mesh->SetMaterial(0, Mat);
	}
	return Mesh;
}

static void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

// ---------------------------------------------------------------- ground marker

AWRGroundMarker::AWRGroundMarker()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Disc = MakeShapeMesh(this, TEXT("Disc"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Box = MakeShapeMesh(this, TEXT("Box"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	// A glowing, pulsing, see-through telegraph (Tools/fx/make_telegraph_material.py): the lit shape material tinted
	// orange blew out to a flat white or yellow patch in strong sun.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Telegraph(TEXT("/Game/FX/M_Telegraph.M_Telegraph"));
	for (UStaticMeshComponent* M : { Disc.Get(), Box.Get() })
	{
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		if (Telegraph.Succeeded()) M->SetMaterial(0, Telegraph.Object);
	}
}

void AWRGroundMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGroundMarker, Size);
	DOREPLIFETIME(AWRGroundMarker, bBox);
	DOREPLIFETIME(AWRGroundMarker, Color);
}

AWRGroundMarker* AWRGroundMarker::Spawn(UWorld* World, const FVector& Location, const FRotator& Rotation, const FVector2D& InSize, bool bInBox, const FLinearColor& InColor, float Seconds)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRGroundMarker* M = World->SpawnActor<AWRGroundMarker>(AWRGroundMarker::StaticClass(), Location, FRotator(0.f, Rotation.Yaw, 0.f), Params);
	if (M)
	{
		M->Size = InSize;
		M->bBox = bInBox;
		M->Color = InColor;
		M->OnRep_Shape();
		if (Seconds > 0.f) M->SetLifeSpan(FMath::Max(0.1f, Seconds));  // 0 = permanent
	}
	return M;
}

void AWRGroundMarker::OnRep_Shape()
{
	Disc->SetVisibility(!bBox);
	Box->SetVisibility(bBox);
	// Engine shapes are 100 units across; markers are 4 cm tall and sit just above the ground.
	Disc->SetRelativeScale3D(FVector(Size.X / 50.f, Size.X / 50.f, 0.04f));
	Box->SetRelativeScale3D(FVector(Size.X / 100.f, Size.Y / 50.f, 0.04f));
	Box->SetRelativeLocation(FVector(Size.X * 0.5f, 0.f, 0.f));
	TintMesh(bBox ? Box : Disc, Color);
}

// ---------------------------------------------------------------- hazard

AWRHazard::AWRHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRHazard* AWRHazard::Spawn(UWorld* World, AActor* InSource, const FVector& Location, float InRadius, float Seconds, float Dps, FName InKind, bool bMarker)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRHazard* H = World->SpawnActor<AWRHazard>(AWRHazard::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!H)
	{
		return nullptr;
	}
	H->Source = InSource;
	H->Radius = InRadius;
	H->DamagePerSecond = Dps;
	H->Kind = InKind;
	if (Seconds > 0.f) H->SetLifeSpan(Seconds);  // 0 = permanent (cave hazards)
	if (bMarker)
	{
		const FLinearColor Color = InKind == TEXT("ice") || InKind == TEXT("frost") ? FLinearColor(0.7f, 0.9f, 1.f) : InKind == TEXT("gas") ? FLinearColor(0.7f, 0.75f, 0.1f) : FLinearColor(1.f, 0.3f, 0.02f);
		H->Marker = AWRGroundMarker::Spawn(World, Location + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector2D(InRadius, InRadius), false, Color, Seconds);
	}
	return H;
}

void AWRHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const AWRCharacterBase* SourceChar = Cast<AWRCharacterBase>(Source.Get());
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || Who == SourceChar || (SourceChar && !SourceChar->IsHostileTo(Who)) || Who->GetAttachParentActor())
		{
			continue;
		}
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
		if (FVector::Dist2D(Feet, GetActorLocation()) > Radius || FMath::Abs(Feet.Z - GetActorLocation().Z) > 600.f)
		{
			continue;
		}
		// Standing on something? Anvils keep you out of the flood; any jump clears a crack for a moment.
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRHazard), false, Who);
		const bool bGrounded = GetWorld()->LineTraceSingleByChannel(Ground, Feet + FVector(0, 0, 20), Feet - FVector(0, 0, 80), ECC_WorldStatic, Query);
		if (!bGrounded || Ground.GetActor()->IsA<AWRBossProp>())
		{
			continue;
		}
		if (Kind == TEXT("ice"))
		{
			Who->Status->ApplyStatus(WRTags::Status_Slowed, 0.6f);
			continue;
		}
		const bool bFrost = Kind == TEXT("frost");
		if (bFrost)
		{
			Who->Status->ApplyStatus(WRTags::Status_Slowed, 0.6f);
		}
		FWRHit Hit;
		Hit.Instigator = Source.Get();
		Hit.Damage = DamagePerSecond * DeltaSeconds * WRTune(TEXT("bosses.damageScale"), 1.f);
		Hit.Element = bFrost ? WRTags::Element_Frost : WRTags::Element_Fire;
		(bFrost ? Hit.FrostBuildup : Hit.BurnBuildup) = (bFrost ? 30.f : 20.f) * DeltaSeconds;
		Hit.bDamageOverTime = true;
		Hit.Origin = GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ---------------------------------------------------------------- props

AWRBossProp::AWRBossProp()
{
	bReplicates = true;
	Mesh = MakeShapeMesh(this, TEXT("Mesh"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	// Stone from the start (anvils), so no prop ever shows the engine's plain white shape material.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(TEXT("/Game/Building/M_BuildStone.M_BuildStone"));
	if (Stone.Succeeded()) Mesh->SetMaterial(0, Stone.Object);
}

void AWRBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBossProp, Kind);
}

void AWRBossProp::Setup(FName InKind)
{
	Kind = InKind;
	OnRep_Kind();
}

void AWRBossProp::OnRep_Kind()
{
	if (Kind == TEXT("pillar"))
	{
		if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			Mesh->SetStaticMesh(Cyl);
		}
		Radius = WRTune(TEXT("bosses.pillarRadius"), 120.f);
		Mesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 8.f));
		// The island's ice (the frozen-ground material); the old pale tint on the shape material read as plain white.
		if (UMaterialInterface* Ice = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Ice.M_Ice"))) Mesh->SetMaterial(0, Ice);
		else TintMesh(Mesh, FLinearColor(0.65f, 0.85f, 1.f));
	}
	else
	{
		// Anvil: a raised stone block to stand on when the floor floods (coursed building stone, set in the constructor).
		Radius = WRTune(TEXT("bosses.anvilHalfSize"), 160.f);
		Mesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 1.5f));
		if (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildStone.M_BuildStone"))) TintMesh(Mesh, FLinearColor(0.18f, 0.16f, 0.15f));
	}
}

void AWRBossProp::Shatter()
{
	UE_LOG(LogWildreach, Log, TEXT("BOSS prop %s (%s) shattered"), *GetName(), *Kind.ToString());
	Destroy();
}

// ---------------------------------------------------------------- boss

AWRBoss::AWRBoss(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AAIController::StaticClass();
	GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);
}

void AWRBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBoss, Phase);
	DOREPLIFETIME(AWRBoss, bEngaged);
	DOREPLIFETIME(AWRBoss, bAirborne);
	DOREPLIFETIME(AWRBoss, Participants);
	DOREPLIFETIME(AWRBoss, ShellFraction);
}

float AWRBoss::Tune(const TCHAR* Key, float Fallback) const
{
	return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *CreatureId.ToString(), Key), Fallback);
}

FGameplayTag AWRBoss::BossElement() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def && Def->Element == TEXT("frost") ? WRTags::Element_Frost : WRTags::Element_Fire;
}

FWRBossAttack AWRBoss::MakeAttack(FName AttackId, EWRBossShape Shape, float Range, float Size, float Windup, float Damage, float Cooldown) const
{
	const FString P = FString::Printf(TEXT("bosses.%s.%s."), *CreatureId.ToString(), *AttackId.ToString());
	const FWRTuning& T = FWRTuning::Get();
	FWRBossAttack A;
	A.Id = AttackId;
	A.Shape = Shape;
	A.Range = T.Num(P + TEXT("range"), Range);
	A.Size = T.Num(P + TEXT("size"), Size);
	A.Windup = T.Num(P + TEXT("windup"), Windup);
	A.Damage = T.Num(P + TEXT("damage"), Damage);
	A.Cooldown = T.Num(P + TEXT("cooldown"), Cooldown);
	A.Poise = T.Num(P + TEXT("poise"), Damage * 0.8f);
	A.bMelee = Shape == EWRBossShape::Cone || Shape == EWRBossShape::Line || Shape == EWRBossShape::SelfCircle;
	return A;
}

void AWRBoss::BuildAttacks()
{
	using S = EWRBossShape;
	Attacks.Reset();
	const auto Add = [this](FWRBossAttack A) -> FWRBossAttack& { return Attacks.Add_GetRef(A); };
	const FName Id = CreatureId;
	if (Id == TEXT("vulkarn"))
	{
		Add(MakeAttack(TEXT("hammer_combo"), S::Cone, 480.f, 55.f, 0.6f, 40.f, 5.f)).Repeats = 3;
		Add(MakeAttack(TEXT("overhead_slam"), S::TargetCircle, 900.f, 280.f, 1.2f, 70.f, 8.f));
		Add(MakeAttack(TEXT("tongs_grab"), S::Line, 750.f, 120.f, 0.9f, 45.f, 10.f));
		Add(MakeAttack(TEXT("anvil_slam"), S::SelfCircle, 2500.f, 450.f, 1.6f, 50.f, 20.f));
		FWRBossAttack& Breath = Add(MakeAttack(TEXT("bellows_breath"), S::Cone, 1000.f, 35.f, 1.0f, 35.f, 9.f));
		Breath.MinPhase = 2; Breath.Buildup = 50.f; Breath.bMelee = false;
		Add(MakeAttack(TEXT("slag_throw"), S::TargetCircle, 1800.f, 220.f, 1.1f, 45.f, 6.f)).MinPhase = 2;
	}
	else if (Id == TEXT("skaldrith"))
	{
		FWRBossAttack& Breath = Add(MakeAttack(TEXT("frost_breath"), S::Cone, 1200.f, 28.f, 1.0f, 40.f, 8.f));
		Breath.Buildup = 45.f; Breath.bMelee = false;
		Add(MakeAttack(TEXT("claw"), S::Cone, 420.f, 60.f, 0.55f, 38.f, 4.f)).Repeats = 2;
		Add(MakeAttack(TEXT("tail_sweep"), S::SelfCircle, 650.f, 650.f, 0.9f, 45.f, 9.f));
		Add(MakeAttack(TEXT("lunge_bite"), S::Line, 900.f, 160.f, 0.8f, 60.f, 8.f));
		FWRBossAttack& Dive = Add(MakeAttack(TEXT("dive"), S::TargetCircle, 3000.f, 350.f, 1.2f, 70.f, 6.f));
		Dive.MinPhase = 2; Dive.Height = 1;
		FWRBossAttack& AirBreath = Add(MakeAttack(TEXT("air_breath"), S::TargetCircle, 3000.f, 450.f, 1.0f, 30.f, 7.f));
		AirBreath.MinPhase = 2; AirBreath.Height = 1; AirBreath.Buildup = 40.f;
		FWRBossAttack& Spikes = Add(MakeAttack(TEXT("ice_spikes"), S::TargetCircle, 3000.f, 200.f, 1.0f, 50.f, 5.f));
		Spikes.MinPhase = 2; Spikes.Height = 2; Spikes.Repeats = 3; Spikes.RepeatGap = 0.4f;
	}
	else if (Id == TEXT("obsidian_warden"))
	{
		Add(MakeAttack(TEXT("ground_pound"), S::SelfCircle, 450.f, 450.f, 1.0f, 45.f, 6.f));
		Add(MakeAttack(TEXT("charge"), S::Line, 1000.f, 150.f, 1.0f, 50.f, 9.f));
		Add(MakeAttack(TEXT("fist"), S::Cone, 380.f, 50.f, 0.7f, 35.f, 3.f));
	}
	else if (Id == TEXT("ignessa"))
	{
		FWRBossAttack& Bolt = Add(MakeAttack(TEXT("fire_bolt"), S::TargetCircle, 2200.f, 180.f, 0.9f, 25.f, 2.5f));
		Bolt.Height = 1; Bolt.Buildup = 30.f;
		Add(MakeAttack(TEXT("swipe"), S::Cone, 400.f, 60.f, 0.6f, 30.f, 3.f));
	}
	else if (Id == TEXT("grimjaw"))
	{
		Add(MakeAttack(TEXT("club"), S::Cone, 450.f, 55.f, 0.8f, 50.f, 5.f)).Repeats = 2;
		Add(MakeAttack(TEXT("boulder"), S::TargetCircle, 1600.f, 260.f, 1.3f, 55.f, 7.f));
		Add(MakeAttack(TEXT("stomp"), S::SelfCircle, 400.f, 400.f, 0.9f, 35.f, 8.f));
	}
	else if (Id == TEXT("pale_hunter"))
	{
		Add(MakeAttack(TEXT("pounce"), S::Line, 800.f, 140.f, 0.7f, 45.f, 6.f));
		Add(MakeAttack(TEXT("claw"), S::Cone, 380.f, 55.f, 0.5f, 32.f, 3.f));
	}
	else
	{
		Add(MakeAttack(TEXT("smash"), S::Cone, 400.f, 50.f, 0.8f, 40.f, 4.f));
	}
}

void AWRBoss::BeginPlay()
{
	Super::BeginPlay();
	const FWRCreatureDef* Def = GetDef();
	bMajor = Def && Def->Role.Contains(TEXT("major"));
	if (!HasAuthority())
	{
		return;
	}
	SoloHealth = Tune(TEXT("health"), WRTune(bMajor ? TEXT("enemies.majorBossSolo.health") : TEXT("enemies.miniBossSolo.health"), bMajor ? 12000.f : 3000.f));
	ArenaRadius = Tune(TEXT("arenaRadius"), bMajor ? 3000.f : 2200.f);
	InitVitals(SoloHealth, 100.f, Tune(TEXT("poise"), bMajor ? 400.f : 200.f));
	BuildAttacks();
	if (CreatureId == TEXT("obsidian_warden"))
	{
		MaxShell = ShellHealth = SoloHealth * Tune(TEXT("shellFraction"), 0.4f);
		ShellFraction = 1.f;
	}
	SpawnArena();
	if (CreatureId == TEXT("ignessa"))
	{
		SetAirborne(true);
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s ready (%s, %.0f hp solo)"), *GetDisplayLabel(), bMajor ? TEXT("major") : TEXT("mini"), SoloHealth);
}

void AWRBoss::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const TWeakObjectPtr<AWRBossProp>& P : Props) if (P.IsValid()) P->Destroy();
	for (const TWeakObjectPtr<AWRCreature>& W : Wisps) if (W.IsValid()) W->Destroy();
	Super::EndPlay(Reason);
}

void AWRBoss::SpawnArena()
{
	for (const TWeakObjectPtr<AWRBossProp>& P : Props) if (P.IsValid()) P->Destroy();
	Props.Reset();
	const bool bAnvils = CreatureId == TEXT("vulkarn");
	if (!bAnvils && CreatureId != TEXT("skaldrith"))
	{
		return;
	}
	const int32 Count = FMath::RoundToInt(Tune(bAnvils ? TEXT("anvils") : TEXT("pillars"), bAnvils ? 6.f : 8.f));
	const float Ring = Tune(TEXT("propRing"), bAnvils ? 1000.f : 1200.f);
	const FVector Floor = GetHome() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = 2.f * PI * (i + 0.5f) / Count;
		FVector At = Floor + FVector(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 0.f);
		// Sit on the ground (the arena may not be flat).
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRBossProp), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Ground, At + FVector(0, 0, 1500), At - FVector(0, 0, 3000), ECC_WorldStatic, Query))
		{
			At = Ground.ImpactPoint;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const float HalfHeight = bAnvils ? 75.f : 400.f;
		if (AWRBossProp* Prop = GetWorld()->SpawnActor<AWRBossProp>(AWRBossProp::StaticClass(), At + FVector(0.f, 0.f, HalfHeight), FRotator::ZeroRotator, Params))
		{
			Prop->Setup(bAnvils ? FName(TEXT("anvil")) : FName(TEXT("pillar")));
			Props.Add(Prop);
		}
	}
}

FString AWRBoss::GetDisplayLabel() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? Def->Name : GetName();
}

bool AWRBoss::AnyArtifactModifiers() const
{
	if (WRTune(TEXT("bosses.forcePhase3"), 0.f) > 0.f)
	{
		return true;
	}
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		const AWRPlayerState* PS = It->GetPlayerState<AWRPlayerState>();
		if (PS && PS->bArtifactModifiers && FVector::Dist(It->GetActorLocation(), GetHome()) < ArenaRadius)
		{
			return true;
		}
	}
	return false;
}

void AWRBoss::NotifyAttackedBy(AActor* Attacker)
{
	if (!bEngaged)
	{
		Engage();
	}
}

void AWRBoss::Engage()
{
	bEngaged = true;
	EmptySince = -1.0;
	UpdateParticipants();
	NextFloodAt = GetWorld()->GetTimeSeconds() + Tune(TEXT("firstFloodSeconds"), 15.f);
	if (CreatureId == TEXT("ignessa"))
	{
		SpawnWisps();
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s engaged by %d participant(s), %.0f hp"), *GetDisplayLabel(), Participants, GetAttributes()->GetMaxHealth());
}

void AWRBoss::UpdateParticipants()
{
	TSet<FString> Players;
	int32 Count = 0;
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerState* PS = It->GetPlayerState();
		const bool bRiding = It->GetAttachParentActor() != nullptr;
		if (PS && (It->IsAlive() || bRiding) && FVector::Dist(It->GetActorLocation(), GetHome()) < ArenaRadius)
		{
			Players.Add(PS->GetPlayerName());
			++Count;
		}
	}
	// Each tame counts as an extra player.
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsTamed() && It->IsAlive() && Players.Contains(It->TamedBy) && FVector::Dist(It->GetActorLocation(), GetHome()) < ArenaRadius)
		{
			++Count;
		}
	}
	if (Count == 0)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (EmptySince < 0.0) EmptySince = Now;
		if (Now - EmptySince > Tune(TEXT("resetSeconds"), 8.f)) ResetFight();
		return;
	}
	EmptySince = -1.0;
	// Health scales up when more join (never down mid-fight); the fraction already dealt is kept.
	const float Scaled = SoloHealth * (1.f + WRTune(TEXT("enemies.bossHealthPerExtraParticipant"), 0.8f) * (Count - 1));
	UWRAttributeSet* A = GetAttributes();
	if (Scaled > A->GetMaxHealth() + 1.f)
	{
		const float Fraction = A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth());
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Scaled);
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Scaled * Fraction);
		if (MaxShell > 0.f)
		{
			const float ShellScale = Scaled / FMath::Max(1.f, MaxShell / Tune(TEXT("shellFraction"), 0.4f));
			MaxShell *= ShellScale;
			ShellHealth *= ShellScale;
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS %s scaled to %d participants: %.0f hp"), *GetDisplayLabel(), Count, Scaled);
	}
	Participants = FMath::Max(Participants, Count);
}

void AWRBoss::ResetFight()
{
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s reset (arena empty)"), *GetDisplayLabel());
	bEngaged = false;
	Participants = 1;
	Ledger.Reset();
	Current = INDEX_NONE;
	SetPhase(1);
	AbilitySystem->RemoveLooseGameplayTag(WRTags::State_Invulnerable, 100);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), SoloHealth);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), SoloHealth);
	if (MaxShell > 0.f)
	{
		MaxShell = ShellHealth = SoloHealth * Tune(TEXT("shellFraction"), 0.4f);
		ShellFraction = 1.f;
	}
	for (const TWeakObjectPtr<AWRCreature>& W : Wisps) if (W.IsValid()) W->Destroy();
	Wisps.Reset();
	bWispsOut = false;
	SetActorHiddenInGame(false);
	SetAirborne(CreatureId == TEXT("ignessa"));
	TeleportTo(GetHome(), GetActorRotation());
	SpawnArena();
}

void AWRBoss::OnRep_Phase()
{
	if (Phase > 1)
	{
		PlayCreatureOneShot(TEXT("roar"));
	}
}

void AWRBoss::SetPhase(int32 NewPhase)
{
	if (NewPhase == Phase)
	{
		return;
	}
	Phase = NewPhase;
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s enters phase %d"), *GetDisplayLabel(), Phase);
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_Phase();  // a listen server or standalone game sees its own boss roar
	}
	if (Phase == 2 && CreatureId == TEXT("vulkarn"))
	{
		SetBodyColor(FLinearColor(1.f, 0.95f, 0.8f));  // white-hot armor
	}
	if (Phase == 2 && CreatureId == TEXT("skaldrith"))
	{
		// Few pillars remain.
		for (int32 i = 0; i < Props.Num(); i += 2) if (Props[i].IsValid()) Props[i]->Shatter();
		SpecialClock = GetWorld()->GetTimeSeconds();
		SetAirborne(true);
	}
	if (Phase == 3 && CreatureId == TEXT("vulkarn"))
	{
		// Quenched rage: plunges into the lava, comes back with a second hammer.
		PlungeUntil = GetWorld()->GetTimeSeconds() + Tune(TEXT("plungeSeconds"), 3.f);
		AbilitySystem->AddLooseGameplayTag(WRTags::State_Invulnerable, 1, EGameplayTagReplicationState::TagOnly);
		SetActorHiddenInGame(true);
		Flood();
	}
	if (Phase == 1)
	{
		SetBodyColor(BaseBodyColor);
	}
}

void AWRBoss::SetAirborne(bool bAir)
{
	bAirborne = bAir;
	GetCharacterMovement()->SetMovementMode(bAir ? MOVE_Flying : MOVE_Walking);
	if (!bAir)
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

void AWRBoss::SpawnWisps()
{
	const int32 Count = FMath::RoundToInt(Tune(TEXT("wisps"), 4.f));
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector At = GetActorLocation() + FVector(FMath::RandPointInCircle(400.f), -200.f);
		if (AWRCreature* Wisp = AWRCreature::SpawnCreature(GetWorld(), TEXT("wisp"), At, FRotator::ZeroRotator))
		{
			Wisps.Add(Wisp);
		}
	}
	bWispsOut = true;
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s sends %d wisps"), *GetDisplayLabel(), Count);
}

void AWRBoss::Flood()
{
	// The floor floods with lava; anvils are safe. Phase 2+ melts one anvil per flood.
	const FVector Floor = GetHome() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	AWRHazard::Spawn(GetWorld(), this, Floor, ArenaRadius, Tune(TEXT("floodSeconds"), 4.f), Tune(TEXT("floodDps"), 30.f), TEXT("flood"));
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s floods the floor"), *GetDisplayLabel());
	if (Phase >= 2)
	{
		for (const TWeakObjectPtr<AWRBossProp>& P : Props)
		{
			if (P.IsValid()) { P->Shatter(); break; }
		}
	}
}

AWRCharacterBase* AWRBoss::PickBossTarget() const
{
	AWRCharacterBase* Best = nullptr;
	float BestDist = ArenaRadius;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It == this || !It->IsAlive() || !IsHostileTo(*It) || It->GetAttachParentActor() || FVector::Dist(It->GetActorLocation(), GetHome()) > ArenaRadius)
		{
			continue;
		}
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

float AWRBoss::ModifyIncomingDamage(const FWRHit& Hit, float Damage)
{
	HealthBeforeHit = GetAttributes()->GetHealth();
	const FWRCreatureDef* Def = GetDef();
	// Weak to the opposite element.
	const FGameplayTag Weak = BossElement() == WRTags::Element_Fire ? WRTags::Element_Frost : WRTags::Element_Fire;
	if (Hit.Element == Weak)
	{
		Damage *= WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f);
	}
	if (CreatureId == TEXT("obsidian_warden") && ShellHealth > 0.f && !Hit.bDamageOverTime)
	{
		// The shell soaks most of it. Chilled shells crack faster.
		const bool bChilled = Hit.Element == WRTags::Element_Frost || HasTag(WRTags::Status_Frostbite) || HasTag(WRTags::Status_Frozen)
			|| (GetAttributes() && GetAttributes()->GetFrostBuildup() > 0.f);
		ShellHealth -= Damage * (bChilled ? Tune(TEXT("chilledShellMultiplier"), 2.5f) : 1.f);
		ShellFraction = FMath::Max(0.f, ShellHealth / FMath::Max(1.f, MaxShell));
		if (ShellHealth <= 0.f)
		{
			ShellHealth = 0.f;
			SetBodyColor(FLinearColor(1.f, 0.55f, 0.1f));  // glowing core exposed
			Status->Stagger(Tune(TEXT("shellBreakStagger"), 3.f));
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s shell broken"), *GetDisplayLabel());
		}
		return Damage * Tune(TEXT("shellDamageTaken"), 0.15f);
	}
	if (CreatureId == TEXT("ignessa") && bAirborne)
	{
		return Damage * Tune(TEXT("airborneDamageTaken"), 0.25f);
	}
	return Damage;
}

void AWRBoss::OnDamageTaken(const FWRHit& Hit, float Damage)
{
	// Ledger: tames' damage counts for their owner.
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	FString Who = By ? By->GetTameOwner() : FString();
	if (Who.IsEmpty() && By && By->GetPlayerState())
	{
		Who = By->GetPlayerState()->GetPlayerName();
	}
	if (!Who.IsEmpty())
	{
		// Overkill doesn't count.
		Ledger.FindOrAdd(Who) += FMath::Min(Damage, HealthBeforeHit);
	}
	if (!bEngaged)
	{
		Engage();
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Skaldrith: hitting his wings in the air grounds him (fire counts double).
	if (CreatureId == TEXT("skaldrith") && bAirborne && !Hit.bDamageOverTime)
	{
		WingHits += Hit.Element == WRTags::Element_Fire ? 2.f : 1.f;
		if (WingHits >= Tune(TEXT("wingHitsToGround"), 6.f))
		{
			WingHits = 0.f;
			SetAirborne(false);
			GroundedUntil = Now + Tune(TEXT("groundedSeconds"), 8.f);
			Status->Stagger(Tune(TEXT("groundedStagger"), 3.f));
			Current = INDEX_NONE;
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s grounded by wing hits"), *GetDisplayLabel());
		}
	}
	// Pale Hunter (and Skaldrith's whiteout): fire hits reveal.
	if (Hit.Element == WRTags::Element_Fire)
	{
		RevealedUntil = Now + Tune(TEXT("revealSeconds"), 6.f);
	}
	// Phases by health.
	const float Fraction = GetAttributes()->GetHealth() / FMath::Max(1.f, GetAttributes()->GetMaxHealth());
	if (Phase == 1 && Fraction < Tune(TEXT("phase2At"), 0.5f) && bMajor)
	{
		SetPhase(2);
	}
	if (Phase == 2 && Fraction < Tune(TEXT("phase3At"), 0.25f) && bMajor && AnyArtifactModifiers())
	{
		SetPhase(3);
	}
}

bool AWRBoss::BreathBlocked(const FVector& Point) const
{
	// Ice pillars block line of sight for Skaldrith's breath.
	const FVector2D From(GetActorLocation()), To(Point);
	for (const TWeakObjectPtr<AWRBossProp>& P : Props)
	{
		if (!P.IsValid() || P->Kind != TEXT("pillar")) continue;
		const FVector2D C(P->GetActorLocation());
		const FVector2D Seg = To - From;
		const float T = FMath::Clamp(FVector2D::DotProduct(C - From, Seg) / FMath::Max(1.f, Seg.SizeSquared()), 0.f, 1.f);
		if (FVector2D::Distance(From + Seg * T, C) < P->Radius && T < 0.98f)
		{
			return true;
		}
	}
	return false;
}

bool AWRBoss::InShape(const FWRBossAttack& Attack, const FVector& Point) const
{
	const FVector Here = GetActorLocation();
	const FVector To = Point - Here;
	const FVector Fwd = AttackFacing.Vector().GetSafeNormal2D();
	switch (Attack.Shape)
	{
	case EWRBossShape::SelfCircle:
		return To.Size2D() <= Attack.Size + 40.f;
	case EWRBossShape::TargetCircle:
		return FVector::Dist2D(Point, AttackPoint) <= Attack.Size + 40.f;
	case EWRBossShape::Cone:
	{
		const float Dist = To.Size2D();
		if (Dist > Attack.Range + GetSimpleCollisionRadius()) return false;
		if (Dist < GetSimpleCollisionRadius() + 60.f) return true;
		const float Cos = FVector::DotProduct(Fwd, To.GetSafeNormal2D());
		return Cos >= FMath::Cos(FMath::DegreesToRadians(Attack.Size));
	}
	case EWRBossShape::Line:
	{
		const float Along = FVector::DotProduct(FVector(To.X, To.Y, 0.f), Fwd);
		const float Side = FMath::Abs(FVector::CrossProduct(Fwd, FVector(To.X, To.Y, 0.f)).Z);
		return Along >= -50.f && Along <= Attack.Range + GetSimpleCollisionRadius() && Side <= Attack.Size + 40.f;
	}
	}
	return false;
}

void AWRBoss::StartAttack(FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	const double Now = GetWorld()->GetTimeSeconds();
	Current = Attacks.IndexOfByPredicate([&Attack](const FWRBossAttack& A) { return A.Id == Attack.Id; });
	RepeatsLeft = Attack.Repeats;
	Attack.ReadyAt = Now + Attack.Cooldown * (CreatureId == TEXT("vulkarn") && Attack.Id == TEXT("anvil_slam") && Phase >= 2 ? 0.6f : 1.f);
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	AttackFacing = FRotator(0.f, ToTarget.Rotation().Yaw, 0.f);
	SetActorRotation(AttackFacing);
	AttackPoint = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
	ResolveAt = Now + Attack.Windup;

	// Telegraph: every attack shows where it lands.
	const FVector Floor = GetActorLocation() - FVector(0.f, 0.f, bAirborne ? 0.f : GetSimpleCollisionHalfHeight());
	const FLinearColor Color = Attack.bMelee ? FLinearColor(1.f, 0.15f, 0.05f) : FLinearColor(1.f, 0.75f, 0.1f);
	switch (Attack.Shape)
	{
	case EWRBossShape::SelfCircle: AWRGroundMarker::Spawn(GetWorld(), Floor, AttackFacing, FVector2D(Attack.Size, Attack.Size), false, Color, Attack.Windup); break;
	case EWRBossShape::TargetCircle: AWRGroundMarker::Spawn(GetWorld(), AttackPoint, AttackFacing, FVector2D(Attack.Size, Attack.Size), false, Color, Attack.Windup); break;
	case EWRBossShape::Cone: AWRGroundMarker::Spawn(GetWorld(), Floor, AttackFacing, FVector2D(Attack.Range, Attack.Range * FMath::Tan(FMath::DegreesToRadians(Attack.Size)) * 0.8f), true, Color, Attack.Windup); break;
	case EWRBossShape::Line: AWRGroundMarker::Spawn(GetWorld(), Floor, AttackFacing, FVector2D(Attack.Range, Attack.Size), true, Color, Attack.Windup); break;
	}
	PlaySwing(EWRSwingKind::Heavy, Attack.Windup + 0.2f);
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s winds up %s at %s"), *GetDisplayLabel(), *Attack.Id.ToString(), *Target->GetName());
}

void AWRBoss::ResolveAttack()
{
	FWRBossAttack& Attack = Attacks[Current];
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bHiddenStrike = IsHidden() && Now > RevealedUntil;
	float Scale = (Phase >= 3 ? Tune(TEXT("phase3DamageScale"), 1.4f) : 1.f) * WRTune(TEXT("bosses.damageScale"), 1.f);
	if (bHiddenStrike && CreatureId == TEXT("pale_hunter"))
	{
		Scale *= Tune(TEXT("ambushMultiplier"), 2.f);  // ambush from cover
		RevealedUntil = Now + 3.0;
	}

	// Movement attacks carry the boss.
	if (Attack.Shape == EWRBossShape::Line && Attack.Id != TEXT("tongs_grab"))
	{
		const FVector End = GetActorLocation() + AttackFacing.Vector().GetSafeNormal2D() * Attack.Range * 0.8f;
		SetActorLocation(End, true);
	}
	if (Attack.Id == TEXT("dive"))
	{
		SetActorLocation(AttackPoint + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() + 20.f), false);
	}

	int32 Hits = 0;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == this || !Who->IsAlive() || !IsHostileTo(Who) || Who->GetAttachParentActor())
		{
			continue;
		}
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
		if (!InShape(Attack, Feet) || (Attack.Id.ToString().Contains(TEXT("breath")) && Attack.Shape == EWRBossShape::Cone && BreathBlocked(Feet)))
		{
			continue;
		}
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = Attack.Damage * Scale;
		Hit.PoiseDamage = Attack.Poise;
		Hit.Element = Attack.Buildup > 0.f || Attack.Id == TEXT("anvil_slam") ? BossElement() : WRTags::Element_Physical;
		(BossElement() == WRTags::Element_Fire ? Hit.BurnBuildup : Hit.FrostBuildup) = Attack.Buildup;
		Hit.bMelee = Attack.bMelee;
		Hit.bHeavy = true;
		Hit.Origin = Attack.Shape == EWRBossShape::TargetCircle ? AttackPoint : GetActorLocation();
		const EWRHitResult Result = UWRCombatLibrary::ApplyHit(Who, Hit);
		Hits += Result == EWRHitResult::Hit ? 1 : 0;
		if (Attack.Id == TEXT("tongs_grab"))
		{
			if (Result == EWRHitResult::Parried)
			{
				// Parrying the tongs staggers him (longer than a normal parry).
				Status->Stagger(Tune(TEXT("grabParryStagger"), 4.f));
				UE_LOG(LogWildreach, Log, TEXT("BOSS %s staggered: tongs grab parried"), *GetDisplayLabel());
			}
			else if (Result == EWRHitResult::Hit)
			{
				Who->Status->Stagger(Tune(TEXT("grabHoldSeconds"), 1.5f));
			}
		}
		if (Phase >= 3 && CreatureId == TEXT("skaldrith") && Attack.Id.ToString().Contains(TEXT("breath")) && Result == EWRHitResult::Hit)
		{
			Who->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s %s lands: %d hit"), *GetDisplayLabel(), *Attack.Id.ToString(), Hits);

	// Aftermath.
	const FVector Floor = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	{
		// The blow lands for everyone to see: dust and rubble where a slam hits the ground (it shakes nearby cameras),
		// a burst where thrown fire or ice strikes.
		const FString Id = Attack.Id.ToString();
		const FVector Centre = Attack.Shape == EWRBossShape::TargetCircle ? AttackPoint : Floor;
		if (Id.Contains(TEXT("slam")) || Id == TEXT("ground_pound") || Id == TEXT("stomp") || Id == TEXT("dive") || Id == TEXT("boulder") || Id == TEXT("tail_sweep"))
		{
			WRFx::Broadcast(this, TEXT("stomp"), Centre, FRotator::ZeroRotator, Attack.Size / 100.f, TEXT("stomp"));
		}
		else if (Id == TEXT("slag_throw") || Id == TEXT("fire_bolt"))
		{
			WRFx::Broadcast(this, TEXT("fire.impact"), Centre, FRotator(90.f, 0.f, 0.f), 2.f, TEXT("fire.impact"));
		}
		else if (Id == TEXT("ice_spikes") || Id == TEXT("air_breath"))
		{
			WRFx::Broadcast(this, TEXT("frostnova"), Centre, FRotator::ZeroRotator, Attack.Size / 100.f, TEXT("frostnova"));
		}
	}
	if (Attack.Id == TEXT("overhead_slam") || Attack.Id == TEXT("slag_throw"))
	{
		AWRHazard::Spawn(GetWorld(), this, AttackPoint, Attack.Id == TEXT("slag_throw") ? 160.f : 250.f, Tune(TEXT("crackSeconds"), 12.f), Tune(TEXT("crackDps"), 10.f), TEXT("lava"));
		if (Phase >= 3 && Attack.Id == TEXT("overhead_slam")) Flood();
	}
	if (Attack.Id == TEXT("anvil_slam"))
	{
		Flood();
	}
	if (Attack.Id == TEXT("air_breath"))
	{
		AWRHazard::Spawn(GetWorld(), this, AttackPoint, Attack.Size, Tune(TEXT("iceSeconds"), 10.f), 0.f, TEXT("ice"));
	}
	if (Attack.Id == TEXT("tail_sweep") || Attack.Id == TEXT("lunge_bite"))
	{
		// Tail and charges shatter pillars they touch.
		for (const TWeakObjectPtr<AWRBossProp>& P : Props)
		{
			if (P.IsValid() && (InShape(Attack, P->GetActorLocation()) || FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < P->Radius + GetSimpleCollisionRadius() + 100.f))
			{
				P->Shatter();
			}
		}
	}

	if (--RepeatsLeft > 0)
	{
		ResolveAt = Now + Attack.RepeatGap;
		if (Attack.Shape == EWRBossShape::TargetCircle)
		{
			// Spikes and combos follow the target.
			if (AWRCharacterBase* Target = PickBossTarget())
			{
				AttackPoint = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
				AWRGroundMarker::Spawn(GetWorld(), AttackPoint, AttackFacing, FVector2D(Attack.Size, Attack.Size), false, FLinearColor(1.f, 0.75f, 0.1f), Attack.RepeatGap);
			}
		}
		return;
	}
	Current = INDEX_NONE;
	BusyUntil = Now + Tune(TEXT("recoverySeconds"), 0.8f);
}

void AWRBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && IsAlive())
	{
		TickCombat(DeltaSeconds);
	}
}

void AWRBoss::TickCombat(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const FWRCreatureDef* Def = GetDef();
	if (!Def)
	{
		return;
	}

	if (!bEngaged)
	{
		// Wake when someone walks into the arena.
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < Def->AggroRadius)
			{
				Engage();
				break;
			}
		}
		return;
	}
	if (Now >= NextParticipantCheck)
	{
		NextParticipantCheck = Now + 1.0;
		UpdateParticipants();
		if (!bEngaged)
		{
			return;
		}
	}

	// --- Boss-specific counterplay ---
	const FName Id = CreatureId;
	if (Id == TEXT("vulkarn"))
	{
		if (PlungeUntil > 0.0 && Now >= PlungeUntil)
		{
			PlungeUntil = 0.0;
			AbilitySystem->RemoveLooseGameplayTag(WRTags::State_Invulnerable, 100);
			SetActorHiddenInGame(false);
			SetBodyColor(FLinearColor(1.f, 0.4f, 0.1f));
			UE_LOG(LogWildreach, Log, TEXT("BOSS Vulkarn returns from the lava with a second hammer"));
		}
		if (PlungeUntil > 0.0)
		{
			return;
		}
		if (Phase >= 2)
		{
			// White-hot armor burns anyone close.
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It != this && It->IsAlive() && IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Tune(TEXT("auraRadius"), 420.f))
				{
					FWRHit Aura;
					Aura.Instigator = this;
					Aura.Damage = Tune(TEXT("auraDps"), 6.f) * DeltaSeconds * WRTune(TEXT("bosses.damageScale"), 1.f);
					Aura.Element = WRTags::Element_Fire;
					Aura.bDamageOverTime = true;
					UWRCombatLibrary::ApplyHit(*It, Aura);
				}
			}
		}
	}
	else if (Id == TEXT("skaldrith") && Phase >= 2)
	{
		// Flight cycle: air for a while, then lands (or is grounded early by wing hits).
		if (bAirborne && Now - SpecialClock > Tune(TEXT("airSeconds"), 14.f) && Current == INDEX_NONE)
		{
			SetAirborne(false);
			SpecialClock = Now;
			GroundedUntil = Now + Tune(TEXT("landedSeconds"), 10.f);
		}
		else if (!bAirborne && Now >= GroundedUntil && Current == INDEX_NONE && !HasTag(WRTags::Status_Staggered))
		{
			SetAirborne(true);
			SpecialClock = Now;
		}
		if (Phase >= 3)
		{
			// Whiteout: he vanishes into the blizzard for most of every cycle.
			const bool bWhiteout = FMath::Fmod(Now, 10.0) < 6.0 && Now > RevealedUntil;
			if (IsHidden() != bWhiteout) SetActorHiddenInGame(bWhiteout);
		}
	}
	else if (Id == TEXT("ignessa"))
	{
		// Wisps explode on contact; when she runs out she lands to make more (the opening).
		for (const TWeakObjectPtr<AWRCreature>& W : Wisps)
		{
			if (!W.IsValid() || !W->IsAlive()) continue;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It != W.Get() && It->IsAlive() && W->IsHostileTo(*It) && *It != this && !It->IsA<AWRBoss>()
					&& FVector::Dist(It->GetActorLocation(), W->GetActorLocation()) < Tune(TEXT("wispBlastRadius"), 200.f))
				{
					FWRHit Blast;
					Blast.Instigator = this;
					Blast.Damage = Tune(TEXT("wispDamage"), 25.f) * WRTune(TEXT("bosses.damageScale"), 1.f);
					Blast.Element = WRTags::Element_Fire;
					Blast.BurnBuildup = 35.f;
					Blast.Origin = W->GetActorLocation();
					UWRCombatLibrary::ApplyHit(*It, Blast);
					W->Die(nullptr);
					break;
				}
			}
		}
		Wisps.RemoveAll([](const TWeakObjectPtr<AWRCreature>& W) { return !W.IsValid() || !W->IsAlive(); });
		if (bWispsOut && Wisps.Num() == 0 && bAirborne)
		{
			bWispsOut = false;
			SetAirborne(false);
			SpecialClock = Now;
			UE_LOG(LogWildreach, Log, TEXT("BOSS Ignessa is out of wisps and lands"));
		}
		if (!bAirborne && !bWispsOut && Now - SpecialClock > Tune(TEXT("landedSeconds"), 8.f) && Current == INDEX_NONE)
		{
			SetAirborne(true);
			SpawnWisps();
		}
	}
	else if (Id == TEXT("grimjaw"))
	{
		// Regenerates unless burning (bleeding joins this in a later tier).
		if (!HasTag(WRTags::Status_Burn))
		{
			UWRAttributeSet* A = GetAttributes();
			const float Regen = A->GetMaxHealth() * Tune(TEXT("regenPerSecond"), 0.015f) * DeltaSeconds;
			AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Regen));
		}
	}
	else if (Id == TEXT("pale_hunter"))
	{
		// Blizzard cycle: invisible during the storm unless a fire hit revealed it; footprints give it away.
		const float Storm = Tune(TEXT("blizzardSeconds"), 14.f), Clear = Tune(TEXT("clearSeconds"), 10.f);
		const bool bBlizzard = FMath::Fmod(Now, Storm + Clear) < Storm;
		const bool bHide = bBlizzard && Now > RevealedUntil;
		if (IsHidden() != bHide)
		{
			SetActorHiddenInGame(bHide);
			UE_LOG(LogWildreach, Log, TEXT("BOSS Pale Hunter %s"), bHide ? TEXT("vanishes into the blizzard") : TEXT("is visible"));
		}
		if (bHide && Now >= NextFootprint && GetVelocity().Size2D() > 50.f)
		{
			NextFootprint = Now + 0.35;
			const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() - 2.f);
			AWRGroundMarker::Spawn(GetWorld(), Feet, GetActorRotation(), FVector2D(18.f, 18.f), false, FLinearColor(0.35f, 0.4f, 0.5f), 4.f);
		}
	}

	// Periodic flood (Vulkarn): in addition to the anvil slam, phase 2 floods more often.
	if (Id == TEXT("vulkarn") && Phase >= 2 && Now >= NextFloodAt && Current == INDEX_NONE)
	{
		NextFloodAt = Now + Tune(TEXT("phase2FloodSeconds"), 22.f);
		Flood();
	}

	// --- Attack state machine ---
	if (HasTag(WRTags::Status_Staggered) || HasTag(WRTags::Status_Frozen))
	{
		Current = INDEX_NONE;
		if (bAirborne && Id == TEXT("skaldrith"))
		{
			SetAirborne(false);
		}
		return;
	}
	if (Current != INDEX_NONE)
	{
		if (Now >= ResolveAt)
		{
			ResolveAttack();
		}
		return;
	}
	AWRCharacterBase* Target = PickBossTarget();
	if (!Target)
	{
		return;
	}
	const float Dist = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());

	if (Now >= BusyUntil)
	{
		// Pick a ready attack that can reach; prefer the big ones.
		TArray<int32> Ready;
		for (int32 i = 0; i < Attacks.Num(); ++i)
		{
			const FWRBossAttack& A = Attacks[i];
			const bool bHeightOk = A.Height == 2 || (A.Height == 1) == bAirborne;
			const float Reach = A.Shape == EWRBossShape::SelfCircle ? A.Size : A.Range;
			if (A.MinPhase <= Phase && bHeightOk && Now >= A.ReadyAt && Dist <= Reach + Target->GetSimpleCollisionRadius() + GetSimpleCollisionRadius())
			{
				Ready.Add(i);
			}
		}
		if (Ready.Num() > 0)
		{
			Ready.Sort([this](int32 L, int32 R) { return Attacks[L].Cooldown > Attacks[R].Cooldown; });
			StartAttack(Attacks[Ready[FMath::RandRange(0, FMath::Min(1, Ready.Num() - 1))]], Target);
			return;
		}
	}

	// Close in (on foot) or hover over the fight (airborne).
	if (bAirborne)
	{
		const FVector Hover = Target->GetActorLocation() + (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D() * 500.f
			+ FVector(0.f, 0.f, Tune(TEXT("hoverHeight"), 650.f));
		SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), Hover, DeltaSeconds, Def->Speed));
		SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	}
	else if (Dist > GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius() + 150.f)
	{
		AddMovementInput((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.f);
	}
}

void AWRBoss::OnDeath(AActor* Killer)
{
	OnCreatureDied.Broadcast(this);
	const FWRCreatureDef* Def = GetDef();
	const float MaxHealth = GetAttributes()->GetMaxHealth();
	const float Needed = MaxHealth * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s defeated (%d participants, %.0f hp)"), *GetDisplayLabel(), Participants, MaxHealth);

	// Everyone who dealt at least 10% gets their own loot.
	for (const TPair<FString, float>& Entry : Ledger)
	{
		AWRPlayerState* PS = nullptr;
		for (APlayerState* P : GetWorld()->GetGameState()->PlayerArray)
		{
			if (P && P->GetPlayerName() == Entry.Key) { PS = Cast<AWRPlayerState>(P); break; }
		}
		if (!PS || !Def)
		{
			continue;
		}
		AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner());
		if (Entry.Value < Needed)
		{
			UE_LOG(LogWildreach, Log, TEXT("BOSS loot: %s dealt %.0f (%.1f%%) - not enough"), *Entry.Key, Entry.Value, 100.f * Entry.Value / MaxHealth);
			if (PC) PC->ClientNotify(FString::Printf(TEXT("%s defeated - you dealt %.0f%%, need %.0f%% for loot"), *Def->Name, 100.f * Entry.Value / MaxHealth, 100.f * Needed / MaxHealth));
			continue;
		}
		FString Got;
		for (const TPair<FName, int32>& L : Def->Loot)
		{
			const int32 N = L.Key == TEXT("upgrade_shard") ? FMath::RandRange(FMath::Max(1, L.Value - 1), L.Value + 1) : L.Value;
			PS->GetInventory()->AddItem(L.Key, N);
			Got += FString::Printf(TEXT("%s x%d  "), *L.Key.ToString(), N);
		}
		if (bMajor && FMath::FRand() < WRTune(TEXT("bosses.partChance"), 0.25f))
		{
			const FName Part = CreatureId == TEXT("vulkarn") ? FName(TEXT("part_giants_ember")) : FName(TEXT("part_wyrm_scale"));
			PS->GetInventory()->AddItem(Part, 1);
			Got += Part.ToString();
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS loot: %s dealt %.0f (%.1f%%) -> %s"), *Entry.Key, Entry.Value, 100.f * Entry.Value / MaxHealth, *Got);
		if (PC) PC->ClientNotify(FString::Printf(TEXT("%s defeated! Loot: %s"), *Def->Name, *Got));
	}
	for (const TWeakObjectPtr<AWRCreature>& W : Wisps) if (W.IsValid()) W->Destroy();
	SetLifeSpan(6.f);
	if (AController* AI = GetController())
	{
		AI->UnPossess();
	}
}
