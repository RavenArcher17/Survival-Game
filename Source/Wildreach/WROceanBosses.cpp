// The Ocean's bosses: Old Barnacle, the Siren Eel, the Reef Tyrant and Thalassor, with their arenas' machines. See
// WROceanBosses.h; cooks/biome-ocean.md 2.13-2.14. Numbers: tuning bosses.<id>.

#include "WROceanBosses.h"
#include "Wildreach.h"
#include "WRAbilitiesTide.h"
#include "WRBoss.h"
#include "WRAltar.h"
#include "WRCharacter.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WROceanCreatures.h"
#include "WROceanTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTide.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRWorldSystems.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "WRStrikeFront.h"

namespace WROceanBossImpl
{
	double ObNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	float ObT(const AWRBoss* Boss, const TCHAR* Key, float Fallback)
	{
		return Boss ? FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback) : Fallback;
	}
	void ObTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	bool Is(const AWRBoss* B, const TCHAR* Id) { return B && B->CreatureId == FName(Id); }
	/** Each boss's server state. */
	struct FState
	{
		// Old Barnacle
		float Shell = 0.f;                  // the shell he wears (hull / bell / carapace), its health
		FName ShellKind;                    // hull, bell, carapace, none
		float ShellTorn = 0.f;              // hull damage since the last plank
		double FireOnShellAt = -1.0;        // the first fire on the shell (3 s and he bolts)
		double NakedUntil = 0.0;
		bool bNaked = false;
		bool bSeeking = false;
		TWeakObjectPtr<AWROceanBossProp> SeekProp;
		double FortressUntil = 0.0;
		TArray<TWeakObjectPtr<AWROceanBossProp>> Spares;
		// The Siren
		double SongUntil = 0.0;
		float CrestDamage = 0.f;
		bool bSongWindup = false;
		TMap<TWeakObjectPtr<AWRCreature>, double> Charmed;
		// The Reef Tyrant
		int32 RouteIndex = 0;
		bool bEverTethered = false;
		double BeachedUntil = 0.0;
		double NextBeachAt = 0.0;
		// Thalassor
		float Beam[4] = { 0.f, 0.f, 0.f, 0.f };
		double DriedUntil[4] = { 0.0, 0.0, 0.0, 0.0 };
		TSet<int32> DriedThisPhase;
		double NextBreathAt = 0.0, BreathUntil = 0.0;
		float BlowholeHealth = 0.f;
		bool bBlowholeBroken = false;
		double ThalBeachedUntil = 0.0;
		float MaelstromBeam = 0.f;
		double MaelstromOffUntil = 0.0;
		double NextVortexLungeAt = 0.0;
		double NextClickAt = 0.0;
		float VortexAngle = 0.f;
	};
	TMap<TWeakObjectPtr<AWRBoss>, FState>& States() { static TMap<TWeakObjectPtr<AWRBoss>, FState> M; return M; }
	FState& St(AWRBoss* B) { return States().FindOrAdd(B); }
	const FState* StC(const AWRBoss* B) { return States().Find(const_cast<AWRBoss*>(B)); }
	UWROceanBody* Body(const AWRBoss* B) { return UWROceanBody::Get(B); }
	void SetFlag(AWRBoss* B, uint32 F, bool bOn) { if (UWROceanBody* Bd = Body(B)) Bd->Set(F, bOn); }
	FVector PoundCentre() { return FWROceanData::Get().PoundAt(); }
	float PoundPoolR() { return FWROceanData::Get().ArenaNum(TEXT("thalassor"), TEXT("pool"), 40.f) * 100.f; }
	float PoundInnerR() { return FWROceanData::Get().PoundRadius() - 200.f; }
	void ObHit(AWRBoss* Boss, AWRCharacterBase* V, float Damage, const FVector& From, const TMap<FName, float>& Buildups = {}, bool bUnblockable = true)
	{
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage;
		Hit.bUnblockable = bUnblockable;
		Hit.bMelee = true;
		Hit.Origin = From;
		Hit.Element = WRTags::Element_Physical;
		for (const TPair<FName, float>& B : Buildups) Hit.Buildups.Add(B.Key, B.Value);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	/** Steer a swimming boss's height toward Z (neutral buoyancy holds it). */
	void SwimToZ(AWRBoss* Boss, float Z)
	{
		const float Dz = Z - Boss->GetActorLocation().Z;
		if (FMath::Abs(Dz) > 40.f) Boss->AddMovementInput(FVector(0.f, 0.f, FMath::Sign(Dz)), FMath::Clamp(FMath::Abs(Dz) / 300.f, 0.2f, 1.f));
	}
	bool Daylight(const UObject* W, float& OutStrength, const AWRBoss* Thal)
	{
		const float H = AWRGameState::HourIn(W);
		OutStrength = 0.f;
		if (H < ObT(Thal, TEXT("dayFrom"), 6.f) || H >= ObT(Thal, TEXT("dayTo"), 20.f)) return false;
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WROcean::Biome());
		if (K && (K->Id == TEXT("fog") || K->Id == TEXT("rain") || K->Id == TEXT("squall"))) return false;
		OutStrength = K && K->Id == TEXT("overcast") ? 0.5f : 1.f;
		return true;
	}
	AWRBoss* FindBoss(const UWorld* W, FName Id)
	{
		for (TActorIterator<AWRBoss> It(W); It; ++It) if (It->CreatureId == Id && It->IsAlive()) return *It;
		return nullptr;
	}
}
using namespace WROceanBossImpl;

// ================================================================================================ the arena props

TArray<TWeakObjectPtr<AWROceanBossProp>>& AWROceanBossProp::All()
{
	static TArray<TWeakObjectPtr<AWROceanBossProp>> L;
	return L;
}

AWROceanBossProp::AWROceanBossProp()
{
	bReplicates = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("HitShape"));
	HitShape->SetupAttachment(RootComponent);
	HitShape->InitSphereRadius(150.f);
	HitShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitShape->SetCollisionObjectType(ECC_WorldDynamic);
	HitShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitShape->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	HitShape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitShape->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitShape->SetGenerateOverlapEvents(false);
	Solid = CreateDefaultSubobject<UBoxComponent>(TEXT("Solid"));
	Solid->SetupAttachment(RootComponent);
	Solid->SetCollisionProfileName(TEXT("BlockAll"));
	Solid->SetBoxExtent(FVector(10.f));
}

void AWROceanBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWROceanBossProp, Kind);
	DOREPLIFETIME(AWROceanBossProp, State);
	DOREPLIFETIME(AWROceanBossProp, Index);
}

void AWROceanBossProp::BeginPlay()
{
	Super::BeginPlay();
	All().RemoveAll([](const TWeakObjectPtr<AWROceanBossProp>& W) { return !W.IsValid(); });
	All().Add(this);
	Build();
}

void AWROceanBossProp::EndPlay(const EEndPlayReason::Type Reason)
{
	All().RemoveAll([this](const TWeakObjectPtr<AWROceanBossProp>& W) { return !W.IsValid() || W.Get() == this; });
	Super::EndPlay(Reason);
}

void AWROceanBossProp::OnRep_Look()
{
	Build();
}

void AWROceanBossProp::SetState(uint8 NewState)
{
	if (State == NewState) return;
	State = NewState;
	Build();
	ForceNetUpdate();
}

bool AWROceanBossProp::Occupied() const
{
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < WRTune(TEXT("bosses.old_barnacle.occupiedReach"), 250.f)) return true;
	}
	return false;
}

bool AWROceanBossProp::TakeHit(AActor* By, float Damage, bool bFire, bool bBlunt)
{
	if (!HasAuthority() || State != 0 || (Kind != TEXT("bell") && Kind != TEXT("carapace"))) return false;
	Health -= Damage * (bBlunt ? 1.5f : 1.f);
	WRFx::Broadcast(this, Kind == TEXT("bell") ? FName(TEXT("hit.spark")) : FName(TEXT("hit.dust")), GetActorLocation() + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.f,
		Kind == TEXT("bell") ? FName(TEXT("ocean.bell_clang")) : FName(TEXT("gather.wood")));
	if (Health <= 0.f)
	{
		SetState(1);
		WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation(), FRotator::ZeroRotator, 2.f, NAME_None);
		ObTell(By, Kind == TEXT("bell") ? TEXT("The bell cracks through: he can't wear it now") : TEXT("The carapace shatters: he can't wear it now"));
	}
	return true;
}

void AWROceanBossProp::Build()
{
	if (BuiltKind == Kind && BuiltState == State && Mesh) return;
	BuiltKind = Kind;
	BuiltState = State;
	if (!Mesh)
	{
		Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("Look"));
		Mesh->SetupAttachment(RootComponent);
		Mesh->RegisterComponent();
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	FWRToolMesh M;
	const bool bTaken = State == 2;
	if (Kind == TEXT("bell"))
	{
		// A Saltkin bronze bell on its side in the sand, 3 m long, green with age, its rim chipped.
		HitShape->SetSphereRadius(170.f);
		if (State == 0) M.Lathe(FVector(-160.f, 0, 110.f), FVector(1, 0, 0), 320.f, { FVector(0.f, 40.f, 40.f), FVector(0.3f, 70.f, 70.f), FVector(0.8f, 110.f, 110.f), FVector(1.f, 130.f, 130.f) }, 18, FLinearColor(FColor(0x4E, 0x8A, 0x6E)) * 0.6f, 0.45f);
		else if (State == 1) for (int32 k = 0; k < 6; ++k) M.Lathe(FVector(-100.f + k * 40.f, (k % 2) * 60.f - 30.f, 30.f), FVector(1, 0.3f, 0.2f).GetSafeNormal(), 60.f, { FVector(0.f, 20.f, 8.f), FVector(1.f, 15.f, 6.f) }, 5, FLinearColor(FColor(0x4E, 0x8A, 0x6E)) * 0.5f, 0.5f);
		Solid->SetBoxExtent(State == 0 ? FVector(150.f, 110.f, 110.f) : FVector(10.f));
		Solid->SetRelativeLocation(FVector(0, 0, 110.f));
	}
	else if (Kind == TEXT("carapace"))
	{
		// A Driftback's empty carapace upturned on the beach: the ridged dome, slate-black with white spots.
		HitShape->SetSphereRadius(200.f);
		if (State == 0)
		{
			M.Lathe(FVector(-220.f, 0, 0), FVector(1, 0, 0), 440.f, { FVector(0.f, 30.f, 20.f), FVector(0.3f, 170.f, 80.f), FVector(0.7f, 180.f, 85.f), FVector(1.f, 40.f, 30.f) }, 18, FLinearColor(FColor(0x1E, 0x20, 0x22)) * 0.6f, 0.6f);
			for (FVector& V : M.Verts) V.Z = FMath::Abs(V.Z);
		}
		else if (State == 1) for (int32 k = 0; k < 7; ++k) M.Lathe(FVector(-150.f + k * 45.f, (k % 3) * 50.f - 50.f, 20.f), FVector(1, 0.4f, 0.1f).GetSafeNormal(), 70.f, { FVector(0.f, 25.f, 6.f), FVector(1.f, 20.f, 5.f) }, 5, FLinearColor(FColor(0x1E, 0x20, 0x22)) * 0.5f, 0.6f);
		Solid->SetBoxExtent(State == 0 ? FVector(200.f, 160.f, 60.f) : FVector(10.f));
		Solid->SetRelativeLocation(FVector(0, 0, 50.f));
	}
	else if (Kind == TEXT("tower"))
	{
		// A Saltkin stilt-tower stump on the Pound's flat: coral-limestone courses 5 m tall, 4 m across its top, a stair up
		// one side. Broken by a breach: a 1 m stub and its rubble.
		HitShape->SetSphereRadius(10.f);
		const FLinearColor Lime = FLinearColor(FColor(0xC9, 0xA4, 0x6A)) * 0.5f;
		const float H = State == 0 ? 520.f : 110.f;
		for (int32 c = 0; c * 50.f < H; ++c) M.Lathe(FVector(0, 0, c * 50.f), FVector::UpVector, 48.f, { FVector(0.f, 285.f - c * 3.f, 285.f - c * 3.f), FVector(1.f, 283.f - c * 3.f, 283.f - c * 3.f) }, 4, Lime * (0.9f + 0.1f * FMath::Sin(c * 1.7f)), 0.9f);
		if (State == 0)
		{
			for (int32 k = 0; k < 14; ++k) M.Lathe(FVector(260.f, -200.f + k * 30.f, k * 36.f), FVector::UpVector, 40.f, { FVector(0.f, 55.f, 20.f), FVector(1.f, 55.f, 20.f) }, 4, Lime * 0.95f, 0.9f);
			Solid->SetBoxExtent(FVector(200.f, 200.f, 260.f));
			Solid->SetRelativeLocation(FVector(0, 0, 260.f));
		}
		else
		{
			for (int32 k = 0; k < 10; ++k) M.Lathe(FVector(FMath::Sin(k * 2.1f) * 380.f, FMath::Cos(k * 1.7f) * 380.f, 0.f), FVector::UpVector, 50.f, { FVector(0.f, 70.f, 50.f), FVector(1.f, 50.f, 40.f) }, 5, Lime * 0.8f, 0.9f);
			Solid->SetBoxExtent(FVector(200.f, 200.f, 55.f));
			Solid->SetRelativeLocation(FVector(0, 0, 55.f));
		}
	}
	if (bTaken) M = FWRToolMesh();
	Mesh->ClearAllMeshSections();
	if (M.Verts.Num() > 0) M.ApplyTo(Mesh);
	if (bTaken) Solid->SetBoxExtent(FVector(1.f));
}

// ================================================================================================ the sun mirrors

TArray<TWeakObjectPtr<AWRSunMirror>>& AWRSunMirror::All()
{
	static TArray<TWeakObjectPtr<AWRSunMirror>> L;
	return L;
}

AWRSunMirror::AWRSunMirror()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(20.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSunMirror::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSunMirror, Operator);
	DOREPLIFETIME(AWRSunMirror, Aim);
	DOREPLIFETIME(AWRSunMirror, Strength);
	DOREPLIFETIME(AWRSunMirror, BeamEnd);
	DOREPLIFETIME(AWRSunMirror, SunstoneLeft);
}

FVector AWRSunMirror::DishCentre() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 260.f);
}

void AWRSunMirror::BuildLook()
{
	bLook = true;
	All().RemoveAll([](const TWeakObjectPtr<AWRSunMirror>& W) { return !W.IsValid(); });
	All().AddUnique(this);
	if (GetNetMode() == NM_DedicatedServer) return;
	Pivot = NewObject<USceneComponent>(this, TEXT("Pivot"));
	Pivot->SetupAttachment(RootComponent);
	Pivot->RegisterComponent();
	Pivot->SetRelativeLocation(FVector(0, 0, 260.f));
	// The dish: polished bronze 3 m across, a shallow bowl, its rim rolled, on a trunnion yoke (the plinth is scenery).
	Dish = NewObject<UProceduralMeshComponent>(this, TEXT("Dish"));
	Dish->SetupAttachment(Pivot);
	Dish->RegisterComponent();
	Dish->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FWRToolMesh M;
	const FLinearColor Bronze = FLinearColor(FColor(0xC8, 0x94, 0x48)) * 0.75f;
	M.Lathe(FVector(-30.f, 0, 0), FVector(1, 0, 0), 40.f, { FVector(0.f, 20.f, 20.f), FVector(0.4f, 120.f, 120.f), FVector(0.9f, 150.f, 150.f), FVector(1.f, 152.f, 152.f) }, 24, Bronze, 0.12f);
	M.Lathe(FVector(-60.f, 0, 0), FVector(1, 0, 0), 30.f, { FVector(0.f, 25.f, 25.f), FVector(1.f, 20.f, 20.f) }, 8, Bronze * 0.6f, 0.4f);
	M.ApplyTo(Dish);
	// The beam: a long thin shaft along +X (scaled to its length), additive gold.
	Beam = NewObject<UProceduralMeshComponent>(this, TEXT("Beam"));
	Beam->SetupAttachment(Pivot);
	Beam->RegisterComponent();
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	FWRToolMesh B;
	B.Lathe(FVector(0, 0, 0), FVector(1, 0, 0), 100.f, { FVector(0.f, 60.f, 60.f), FVector(1.f, 40.f, 40.f) }, 10, FLinearColor::White, 0.3f);
	B.ApplyTo(Beam);
	if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow")))
	{
		BeamMat = UMaterialInstanceDynamic::Create(G, this);
		BeamMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.8f, 0.4f));
		BeamMat->SetScalarParameterValue(TEXT("Glow"), 6.f);
		Beam->SetMaterial(0, BeamMat);
	}
	Beam->SetVisibility(false);
}

void AWRSunMirror::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLook) BuildLook();
	if (HasAuthority()) ServerTick(DeltaSeconds);
	if (Pivot)
	{
		Pivot->SetWorldRotation(FMath::RInterpTo(Pivot->GetComponentRotation(), Aim, DeltaSeconds, 10.f));
		const bool bOn = Strength > 0.f && !Operator.IsEmpty();
		Beam->SetVisibility(bOn);
		if (bOn)
		{
			const float Len = FMath::Max(100.f, FVector::Dist(DishCentre(), BeamEnd));
			Beam->SetRelativeScale3D(FVector(Len / 100.f, Strength, Strength));
			if (BeamMat) BeamMat->SetScalarParameterValue(TEXT("Glow"), 3.f + 4.f * Strength);
		}
	}
}

void AWRSunMirror::ServerTick(float Dt)
{
	APawn* Op = OperatorPawn.Get();
	// The operator stays at the mirror (3 m), alive; else it's let go.
	if (!Operator.IsEmpty() && (!Op || FVector::Dist2D(Op->GetActorLocation(), GetActorLocation()) > WRTune(TEXT("bosses.thalassor.mirrorReach"), 300.f) + 150.f
		|| (Cast<AWRCharacterBase>(Op) && !Cast<AWRCharacterBase>(Op)->IsAlive())))
	{
		Operator.Empty();
		OperatorPawn = nullptr;
		Strength = 0.f;
	}
	if (Operator.IsEmpty()) { Strength = 0.f; return; }
	Aim = Op->GetBaseAimRotation();
	AWRBoss* Thal = FindBoss(GetWorld(), TEXT("thalassor"));
	float Sun = 0.f;
	Daylight(this, Sun, Thal);
	if (Sun <= 0.f && SunstoneLeft > 0.f) { Sun = 1.f; SunstoneLeft = FMath::Max(0.f, SunstoneLeft - Dt); }
	Strength = Sun;
	const FVector From = DishCentre();
	const FVector To = From + Aim.Vector() * WRTune(TEXT("bosses.thalassor.mirrorBeamRange"), 30000.f);
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSunBeam), false, this);
	Q.AddIgnoredActor(Op);
	BeamEnd = GetWorld()->LineTraceSingleByChannel(H, From, To, ECC_Visibility, Q) ? H.ImpactPoint : To;
	if (Sun <= 0.f || !Thal) return;
	// On Thalassor: the beam's nearest part (the body runs far past its capsule: measured along the beam).
	const FVector Dir = Aim.Vector();
	float Best = 1.0e9f;
	int32 BestPart = -1;
	const FVector Fwd = Thal->GetActorForwardVector(), C = Thal->GetActorLocation();
	const FVector Parts[4] = { C + Fwd * 700.f, C + Fwd * 1150.f + FVector(0, 0, 250.f), C - Fwd * 1500.f, C - Fwd * 4100.f };
	for (int32 p = 0; p < 4; ++p)
	{
		const FVector Q2 = FMath::ClosestPointOnSegment(Parts[p], From, BeamEnd);
		const float D = FVector::Dist(Q2, Parts[p]);
		if (D < Best) { Best = D; BestPart = p; }
	}
	if (BestPart >= 0 && Best < 450.f) FWROceanBoss::AddBeam(Thal, BestPart, Dt * Sun);
}

// ================================================================================================ the hooks

bool FWROceanBoss::IsOceanBoss(const AWRBoss* Boss)
{
	return Boss && (Is(Boss, TEXT("old_barnacle")) || Is(Boss, TEXT("siren_eel")) || Is(Boss, TEXT("reef_tyrant")) || Is(Boss, TEXT("thalassor")));
}

void FWROceanBoss::Begin(AWRBoss* Boss)
{
	if (!IsOceanBoss(Boss)) return;
	FState& S = St(Boss);
	S = FState();
	if (UWRCharacterMovementComponent* M = Cast<UWRCharacterMovementComponent>(Boss->GetCharacterMovement()))
	{
		M->bHoldDepth = !Is(Boss, TEXT("old_barnacle"));
		if (Boss->GetDef() && Boss->GetDef()->SwimSpeed > 0.f) M->MaxSwimSpeed = Boss->GetDef()->SwimSpeed;
	}
	if (Is(Boss, TEXT("old_barnacle")))
	{
		S.ShellKind = TEXT("hull");
		S.Shell = ObT(Boss, TEXT("hullHealth"), 1200.f);
		if (Boss->HasAuthority()) if (UWROceanArenas* A = UWROceanArenas::Get(Boss)) A->SpawnSpares(Boss);
	}
	if (Is(Boss, TEXT("thalassor")))
	{
		S.BlowholeHealth = ObT(Boss, TEXT("blowholeHealth"), 1000.f);
		S.NextBreathAt = ObNow(Boss) + ObT(Boss, TEXT("breathEvery"), 30.f);
	}
}

void FWROceanBoss::Reset(AWRBoss* Boss)
{
	if (!IsOceanBoss(Boss)) return;
	FState& S = St(Boss);
	for (const TPair<TWeakObjectPtr<AWRCreature>, double>& C : S.Charmed) if (C.Key.IsValid()) C.Key->SetAggroTarget(nullptr);
	const TArray<TWeakObjectPtr<AWROceanBossProp>> Spares = S.Spares;
	Begin(Boss);
	S.Spares = Spares;
	for (const TWeakObjectPtr<AWROceanBossProp>& P : S.Spares) if (P.IsValid()) { P->SetState(0); P->Health = P->MaxHealth; }
	if (UWROceanBody* Bd = Body(Boss)) Bd->Flags = 0;
	if (Is(Boss, TEXT("thalassor"))) if (UWROceanArenas* A = UWROceanArenas::Get(Boss)) A->ResetPound();
	Boss->GetCharacterMovement()->MaxWalkSpeed = Boss->GetDef() ? Boss->GetDef()->Speed : 400.f;
}

void FWROceanBoss::EndPlay(AWRBoss* Boss)
{
	if (!IsOceanBoss(Boss)) return;
	States().Remove(Boss);
}

bool FWROceanBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	if (Is(Boss, TEXT("reef_tyrant")))
	{
		// Its loop: lighthouse, the Blue Wall, the Whale Fall, along the drop, over the reef, the Mooring Stones (rest
		// 13:00-15:00: sharks rest), out along the Long Dyke, back.
		const FWROceanData& D = FWROceanData::Get();
		TArray<FVector> Route = D.ArenaPoints(TEXT("reef_tyrant"), TEXT("route"));
		const float H = AWRGameState::HourIn(Boss);
		FVector Rest;
		const bool bRest = H >= ObT(Boss, TEXT("restFrom"), 13.f) && H < ObT(Boss, TEXT("restTo"), 15.f) && D.ArenaPoint(TEXT("reef_tyrant"), TEXT("rest"), Rest);
		FVector Want;
		if (bRest) Want = FVector(Rest.X, Rest.Y, WRTide::SurfaceAt(Boss, Rest) - 250.f);
		else if (Route.Num() > 0)
		{
			S.RouteIndex = S.RouteIndex % Route.Num();
			Want = Route[S.RouteIndex];
			Want.Z = FMath::Min(Want.Z, WRTide::SurfaceAt(Boss, Want) - ObT(Boss, TEXT("cruiseDepth"), 600.f));
			if (FVector::Dist2D(Want, Boss->GetActorLocation()) < 1500.f) S.RouteIndex = (S.RouteIndex + 1) % Route.Num();
		}
		else return false;
		const FVector Dd = Want - Boss->GetActorLocation();
		if (Dd.Size2D() > 300.f)
		{
			Boss->AddMovementInput(Dd.GetSafeNormal2D(), bRest ? 0.25f : ObT(Boss, TEXT("routeInput"), 0.55f));
			Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), Dd.GetSafeNormal2D().Rotation(), DeltaSeconds, 1.5f));
		}
		SwimToZ(Boss, FMath::Max(Want.Z, WRLand::GroundZAt(Boss->GetActorLocation(), Want.Z - 2000.f) + 300.f));
		Boss->SetHome(Boss->GetActorLocation());
		return true;
	}
	if (Is(Boss, TEXT("thalassor")))
	{
		// It lies in the pool, turning slow circles, clicking.
		const FVector C = PoundCentre();
		S.VortexAngle += DeltaSeconds * 0.05f;
		const FVector Want = C + FVector(FMath::Cos(S.VortexAngle), FMath::Sin(S.VortexAngle), 0.f) * PoundPoolR() * 0.4f;
		Boss->AddMovementInput((Want - Boss->GetActorLocation()).GetSafeNormal2D(), 0.2f);
		SwimToZ(Boss, WRTide::SurfaceAt(Boss, C) - 350.f);
		return true;
	}
	if (Is(Boss, TEXT("siren_eel")))
	{
		// She coils in her lake, deep.
		SwimToZ(Boss, WRTide::SurfaceAt(Boss, Boss->GetHome()) - 500.f);
		return false;
	}
	return false;
}

bool FWROceanBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const double T = ObNow(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	UWROceanBody* Bd = Body(Boss);
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	// ---------------------------------------------------------------- Old Barnacle
	if (Is(Boss, TEXT("old_barnacle")))
	{
		// Fire on the shell 3 s: he bolts out naked.
		if (!S.bNaked && S.FireOnShellAt > 0.0 && T - S.FireOnShellAt >= ObT(Boss, TEXT("fireBurnSeconds"), 3.f))
		{
			S.Shell = 0.f;
		}
		if (!S.bNaked && S.Shell <= 0.f && S.ShellKind != NAME_None)
		{
			S.bNaked = true;
			S.ShellKind = NAME_None;
			S.NakedUntil = T + ObT(Boss, TEXT("nakedSeconds"), 12.f);
			S.FireOnShellAt = -1.0;
			SetFlag(Boss, WROceanFlag::Bands, true);
			SetFlag(Boss, WROceanFlag::Guard, false);
			WRFx::Broadcast(Boss, TEXT("hit.dust"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("boss.old_barnacle.naked"));
			Boss->GetCharacterMovement()->MaxWalkSpeed = Boss->GetDef()->Speed * 1.2f;
			UE_LOG(LogWildreach, Log, TEXT("OCEAN Old Barnacle is out of his shell"));
		}
		// Naked time over: he runs for a spare (not broken, not taken, nobody standing on it), or stays naked.
		if (S.bNaked && !S.bSeeking && T >= S.NakedUntil)
		{
			AWROceanBossProp* Best = nullptr;
			float BestD = 1.0e9f;
			for (const TWeakObjectPtr<AWROceanBossProp>& W : S.Spares)
			{
				AWROceanBossProp* P = W.Get();
				if (!P || !P->IsLive()) continue;
				const float D = FVector::Dist2D(P->GetActorLocation(), Boss->GetActorLocation());
				if (D < BestD) { BestD = D; Best = P; }
			}
			if (Best) { S.bSeeking = true; S.SeekProp = Best; }
			else S.NakedUntil = T + 1.0e6;   // nothing left: naked for good
		}
		if (S.bSeeking)
		{
			AWROceanBossProp* P = S.SeekProp.Get();
			if (!P || !P->IsLive()) { S.bSeeking = false; S.NakedUntil = T; return false; }
			if (P->Occupied())
			{
				// Someone's standing on it: he goes for them (the pinch to get them off).
				return false;
			}
			const FVector D = P->GetActorLocation() - Boss->GetActorLocation();
			if (D.Size2D() > ObT(Boss, TEXT("spareReach"), 300.f))
			{
				Boss->AddMovementInput(D.GetSafeNormal2D(), 1.f);
				Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), D.GetSafeNormal2D().Rotation(), DeltaSeconds, 4.f));
				return true;
			}
			// In it: the spare becomes his shell.
			S.bSeeking = false;
			S.bNaked = false;
			S.ShellKind = P->Kind;
			S.Shell = P->Health;
			S.ShellTorn = 0.f;
			P->SetState(2);
			SetFlag(Boss, WROceanFlag::Bands, false);
			Boss->GetCharacterMovement()->MaxWalkSpeed = Boss->GetDef()->Speed * (P->Kind == TEXT("bell") ? ObT(Boss, TEXT("bellSpeed"), 0.7f) : ObT(Boss, TEXT("carapaceSpeed"), 1.2f));
			WRFx::Broadcast(Boss, TEXT("hit.dust"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("boss.old_barnacle.reshell"));
			return true;
		}
		if (T < S.FortressUntil) { Move->StopMovementImmediately(); return true; }
		SetFlag(Boss, WROceanFlag::Guard, false);
		return false;
	}
	// ---------------------------------------------------------------- the Siren Eel
	if (Is(Boss, TEXT("siren_eel")))
	{
		// The song's charmed tames: they leave the fight and come to her (the Siren scale on their owner keeps them).
		for (auto It = S.Charmed.CreateIterator(); It; ++It)
		{
			AWRCreature* C = It.Key().Get();
			if (!C || T > It.Value() || !C->IsAlive()) { if (C) C->SetAggroTarget(nullptr); It.RemoveCurrent(); continue; }
			C->SetAggroTarget(nullptr);
			C->AddMovementInput((Boss->GetActorLocation() - C->GetActorLocation()).GetSafeNormal2D(), 1.f);
		}
		// Her depth: at the surface by a rock to sing, under otherwise.
		if (Move->IsSwimming()) SwimToZ(Boss, WRTide::SurfaceAt(Boss, Boss->GetActorLocation()) - (T < S.SongUntil || S.bSongWindup ? 0.f : 250.f));
		return false;
	}
	// ---------------------------------------------------------------- the Reef Tyrant
	if (Is(Boss, TEXT("reef_tyrant")))
	{
		if (T < S.BeachedUntil) { Move->StopMovementImmediately(); return true; }
		if (Bd && Bd->Has(WROceanFlag::Flipped)) Bd->Set(WROceanFlag::Flipped, false);
		// Tied off: it can't go past the leash from the stone; two tethers in shallow water beach it.
		const TArray<AWRHarpoonLine*> Lines = AWRHarpoonLine::On(Boss);
		int32 Tied = 0;
		for (AWRHarpoonLine* L : Lines)
		{
			if (!L || !L->bTied) continue;
			++Tied;
			S.bEverTethered = true;
			const FVector D = Boss->GetActorLocation() - L->TiedAt;
			const float Leash = ObT(Boss, TEXT("leash"), 1000.f);
			if (D.Size() > Leash)
			{
				Move->Velocity = -D.GetSafeNormal() * 400.f;
				Boss->SetActorLocation(L->TiedAt + D.GetSafeNormal() * Leash, true);
				L->AddPull(DeltaSeconds * 60.f);
				if (FMath::Fmod(float(T), 0.6f) < DeltaSeconds) WRFx::Broadcast(Boss, TEXT("ocean.splash"), Boss->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("tide.rope_creak"));
			}
		}
		const float Depth = WRTide::SurfaceAt(Boss, Boss->GetActorLocation()) - WRLand::GroundZAt(Boss->GetActorLocation(), -100000.f);
		if (Tied >= 2 && Depth < ObT(Boss, TEXT("beachDepth"), 200.f) && T >= S.NextBeachAt)
		{
			S.BeachedUntil = T + ObT(Boss, TEXT("beachSeconds"), 8.f);
			S.NextBeachAt = S.BeachedUntil + 6.0;
			Boss->StanceOpenUntil = S.BeachedUntil;
			if (Bd) Bd->Set(WROceanFlag::Flipped, true);
			Boss->Current = INDEX_NONE;
			Boss->StrikeState = AWRBoss::EStrike::None;
			WRFx::Broadcast(Boss, TEXT("ocean.splash"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("creature.reefjaw.breach"));
			UE_LOG(LogWildreach, Log, TEXT("OCEAN the Reef Tyrant is beached"));
			return true;
		}
		if (Move->IsSwimming() && Target) SwimToZ(Boss, Target->GetActorLocation().Z);
		return false;
	}
	// ---------------------------------------------------------------- Thalassor
	if (Is(Boss, TEXT("thalassor")))
	{
		// Its clicks the whole fight (sperm-whale sonar).
		if (T >= S.NextClickAt) { S.NextClickAt = T + 2.5; WRSound::PlayAt(Boss, TEXT("boss.thalassor.click"), Boss->GetActorLocation()); }
		// Beached: rolled on the flat, the belly up.
		if (T < S.ThalBeachedUntil) { Move->StopMovementImmediately(); return true; }
		if (Bd && Bd->Has(WROceanFlag::Flipped)) Bd->Set(WROceanFlag::Flipped, false);
		// The blowhole's breath: every 30 s it surfaces for 4 s (a 12 m plume), the blowhole open.
		if (T >= S.NextBreathAt && Boss->Phase >= 2 && !S.bBlowholeBroken)
		{
			S.NextBreathAt = T + ObT(Boss, TEXT("breathEvery"), 30.f);
			S.BreathUntil = T + ObT(Boss, TEXT("breathSeconds"), 4.f);
			WRFx::Broadcast(Boss, TEXT("ocean.thal_plume"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * 1150.f + FVector(0, 0, 300.f), FRotator::ZeroRotator, 2.f, TEXT("boss.thalassor.breath"));
		}
		const FVector C = PoundCentre();
		const float Surface = WRTide::SurfaceAt(Boss, C);
		const float Flood = UWROceanArenas::FloodLevel(Boss);
		const float WaterTop = FMath::Max(Surface, Flood);
		// Phase 3, the Maelstrom: the whole Pound spins; swimmers drawn to the eye; it rides the vortex wall and lunges at a
		// perch every 6 s; 3 s of beam breaks it 8 s.
		if (Boss->Phase >= 3 && T >= S.MaelstromOffUntil)
		{
			const float R = ObT(Boss, TEXT("maelstromRadius"), 3000.f);
			S.VortexAngle += DeltaSeconds * 0.35f;
			const FVector Wall = C + FVector(FMath::Cos(S.VortexAngle), FMath::Sin(S.VortexAngle), 0.f) * R;
			Boss->AddMovementInput((Wall - Boss->GetActorLocation()).GetSafeNormal2D(), 1.f);
			SwimToZ(Boss, WaterTop - 200.f);
			if (FMath::Fmod(float(T), 0.25f) < DeltaSeconds)
			{
				for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
				{
					AWRCharacterBase* V = *It;
					if (V == Boss || !V->IsAlive() || UWROceanBody::Get(V) || !V->GetCharacterMovement()->IsSwimming() || FVector::Dist2D(V->GetActorLocation(), C) > PoundInnerR()) continue;
					const FVector In = (C - V->GetActorLocation()).GetSafeNormal2D();
					WRSkyAir::Push(V, (In + FVector(-In.Y, In.X, 0.f) * 0.6f) * ObT(Boss, TEXT("maelstromPull"), 300.f), 0.25f, TEXT("maelstrom"), Boss);
					if (FVector::Dist2D(V->GetActorLocation(), C) < 600.f) ObHit(Boss, V, ObT(Boss, TEXT("maelstromEyeDps"), 20.f) * 0.25f, C);
				}
				WRFx::Broadcast(Boss, TEXT("tide.maelstrom"), FVector(C.X, C.Y, WaterTop), FRotator::ZeroRotator, R / 1000.f, NAME_None, 0.3f);
			}
			if (T >= S.NextVortexLungeAt && Target)
			{
				S.NextVortexLungeAt = T + ObT(Boss, TEXT("maelstromLungeEvery"), 6.f);
				for (FWRBossAttack& A : Boss->Attacks) if (A.Id == TEXT("head_ram")) { A.ReadyAt = 0.0; break; }
			}
			return false;
		}
		// Phase 1: held to the pool (it lunges out onto the flat, then swims back); its height under the water.
		if (Boss->Phase <= 1 && Boss->StrikeState == AWRBoss::EStrike::None)
		{
			const FVector D = C - Boss->GetActorLocation();
			if (D.Size2D() > PoundPoolR() * 0.85f) Boss->AddMovementInput(D.GetSafeNormal2D(), 1.f);
		}
		if (Move->IsSwimming()) SwimToZ(Boss, T < S.BreathUntil ? WaterTop - 100.f : WaterTop - 300.f);
		return false;
	}
	return false;
}

bool FWROceanBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsOceanBoss(Boss)) return true;
	const FState* S = StC(Boss);
	const FName Id = Attack.Id;
	const double T = ObNow(Boss);
	if (Is(Boss, TEXT("old_barnacle")) && S)
	{
		// Naked: only the pinch and the charge. In his fortress: nothing. The fortress below 60% or flanked by two.
		if (S->bNaked || S->bSeeking) return Id == TEXT("crusher_pinch") || Id == TEXT("shell_charge");
		if (T < S->FortressUntil) return false;
		if (Id == TEXT("hull_fortress"))
		{
			const float Frac = Boss->GetHealthFraction();
			int32 Flank = 0;
			for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
			{
				const FVector D = It->GetActorLocation() - Boss->GetActorLocation();
				if (It->IsAlive() && D.Size2D() < 600.f && FMath::Abs(FVector::DotProduct(D.GetSafeNormal2D(), Boss->GetActorForwardVector())) < 0.5f) ++Flank;
			}
			return Frac < ObT(Boss, TEXT("fortressBelow"), 0.6f) || Flank >= 2;
		}
		if (Id == TEXT("debris_toss")) return Target && FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) > ObT(Boss, TEXT("tossMin"), 600.f);
		return true;
	}
	if (Is(Boss, TEXT("siren_eel")))
	{
		// She must be on (by) a rock island to sing.
		if (Id == TEXT("siren_song"))
		{
			const FWROceanData::FCave* Cv = FWROceanData::Get().FindCave(TEXT("sirens_hollow"));
			if (!Cv) return false;
			for (const FVector& I : Cv->Islands)
			{
				const FVector At = Cv->At + FVector(I.X * 100.f, I.Y * 100.f, 0.f);
				if (FVector::Dist2D(At, Boss->GetActorLocation()) < I.Z * 100.f + ObT(Boss, TEXT("islandReach"), 1500.f)) return true;
			}
			return false;
		}
		if (Id == TEXT("second_jaw")) return Target && !WRUnderwater::IsUnder(Target);   // at the water's edge
		return true;
	}
	if (Is(Boss, TEXT("reef_tyrant")) && S)
	{
		if (T < S->BeachedUntil) return false;
		if (Id == TEXT("breach")) return Target && Target->GetActorLocation().Z > WRTide::SurfaceAt(Boss, Target->GetActorLocation()) - 200.f && Boss->GetActorLocation().Z < WRTide::SurfaceAt(Boss, Boss->GetActorLocation()) - 300.f;
		return true;
	}
	if (Is(Boss, TEXT("thalassor")) && S)
	{
		if (T < S->ThalBeachedUntil) return false;
		if (Id == TEXT("bubble_net")) return Boss->GetHealthFraction() < 0.75f;
		if (Id == TEXT("spout_volley")) return !S->bBlowholeBroken;
		if (Boss->Phase >= 3 && T < S->MaelstromOffUntil) return Id == TEXT("jaw_snap") || Id == TEXT("head_ram");
		return true;
	}
	return true;
}

void FWROceanBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	if (Is(Boss, TEXT("old_barnacle")))
	{
		if (Id == TEXT("crusher_pinch") || Id == TEXT("pinch_hold")) SetFlag(Boss, WROceanFlag::Cocked, true);
		if (Id == TEXT("shell_charge")) WRSound::PlayAt(Boss, TEXT("creature.spinecrab.drum"), Boss->GetActorLocation());
		if (Id == TEXT("hull_slam")) SetFlag(Boss, WROceanFlag::Guard, true);
		if (Id == TEXT("debris_toss")) WRSound::PlayAt(Boss, TEXT("boss.old_barnacle.dig"), Boss->GetActorLocation());
	}
	else if (Is(Boss, TEXT("siren_eel")))
	{
		if (Id == TEXT("siren_song"))
		{
			// She rises straight up, the crest flares, a rising melody.
			S.bSongWindup = true;
			S.CrestDamage = 0.f;
			SetFlag(Boss, WROceanFlag::Flare, true);
			WRFx::Broadcast(Boss, TEXT("ocean.siren_crest"), Boss->GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 2.f, TEXT("boss.siren_eel.song"), Attack.Windup + Attack.Row.Active);
		}
		if (Id == TEXT("second_jaw")) SetFlag(Boss, WROceanFlag::Gape, true);
		if (Id == TEXT("shatter_shriek")) WRSound::PlayAt(Boss, TEXT("boss.siren_eel.shriek"), Boss->GetActorLocation());
	}
	else if (Is(Boss, TEXT("reef_tyrant")))
	{
		if (Id == TEXT("slingshot_bite") || Id == TEXT("thrash_drag")) SetFlag(Boss, WROceanFlag::Gape, true);
		if (Id == TEXT("breach") && Target) WRFx::Broadcast(Boss, TEXT("ocean.bubble_column"), Target->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("creature.reefjaw.thrum"), Attack.Windup + Attack.Row.Hold);
		if (Id == TEXT("ram")) WRSound::PlayAt(Boss, TEXT("creature.reefjaw.thrum"), Boss->GetActorLocation());
	}
	else if (Is(Boss, TEXT("thalassor")))
	{
		// The creak before every move; before a grab, silence.
		if (!Attack.bGrab) WRSound::PlayAt(Boss, TEXT("boss.thalassor.creak"), Boss->GetActorLocation());
		else S.NextClickAt = ObNow(Boss) + Attack.Windup + 1.0;
		if (Id == TEXT("swallow_lunge") || Id == TEXT("jaw_snap")) SetFlag(Boss, WROceanFlag::Gape, true);
		if (Id == TEXT("sluice_break")) SetFlag(Boss, WROceanFlag::Flare, true);
		if (Id == TEXT("wave_wash")) WRFx::Broadcast(Boss, TEXT("ocean.surge_tell"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * 800.f, Boss->GetActorRotation(), 3.f, TEXT("ocean.surge"));
		if (Id == TEXT("fluke_slam")) WRFx::Broadcast(Boss, TEXT("ocean.blowhole_column"), Boss->GetActorLocation() - Boss->GetActorForwardVector() * 4100.f, FRotator::ZeroRotator, 2.f, NAME_None);
		if (Id == TEXT("spout_volley")) WRSound::PlayAt(Boss, TEXT("boss.thalassor.whistle"), Boss->GetActorLocation());
		if (Id == TEXT("bubble_net") && Target) WRFx::Broadcast(Boss, TEXT("ocean.bubble_net"), Target->GetActorLocation(), FRotator::ZeroRotator, 1.f, NAME_None, 3.0f);
		if (Id == TEXT("undertow") && Target) WRFx::Broadcast(Boss, TEXT("tide.whirlpool"), Target->GetActorLocation(), FRotator::ZeroRotator, 15.f, TEXT("tide.whirlpool"), Attack.Windup + Attack.Row.Active);
	}
}

bool FWROceanBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = ObNow(Boss);
	if (Is(Boss, TEXT("old_barnacle")) && Id == TEXT("hull_fortress"))
	{
		S.FortressUntil = T + Attack.Row.Active;
		SetFlag(Boss, WROceanFlag::Guard, true);
		WRSound::PlayAt(Boss, TEXT("boss.old_barnacle.withdraw"), Boss->GetActorLocation());
		return true;
	}
	if (Is(Boss, TEXT("siren_eel")) && Id == TEXT("siren_song"))
	{
		S.bSongWindup = false;
		S.SongUntil = T + ObT(Boss, TEXT("songSeconds"), 8.f);
		// Tames within 30 m turn (the furthest first; all of them in reach): unless their owner wears the Siren scale.
		TArray<AWRCreature*> Tames;
		for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsTamed() || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > ObT(Boss, TEXT("songTameReach"), 3000.f)) continue;
			bool bProof = false;
			for (FConstPlayerControllerIterator P = Boss->GetWorld()->GetPlayerControllerIterator(); P; ++P)
			{
				const APawn* Pw = P->Get() ? P->Get()->GetPawn() : nullptr;
				if (Pw && Pw->GetPlayerState() && Pw->GetPlayerState()->GetPlayerName() == It->GetTameOwner() && UWRInventoryComponent::GearStatFor(Pw, TEXT("tameSteady")) > 0.f) bProof = true;
			}
			if (!bProof) Tames.Add(*It);
		}
		Tames.Sort([Boss](const AWRCreature& A, const AWRCreature& B) { return FVector::Dist(A.GetActorLocation(), Boss->GetActorLocation()) > FVector::Dist(B.GetActorLocation(), Boss->GetActorLocation()); });
		for (AWRCreature* C : Tames) S.Charmed.Add(C, S.SongUntil);
		UE_LOG(LogWildreach, Log, TEXT("OCEAN the Siren sings: %d tames turn"), Tames.Num());
		return true;
	}
	if (Is(Boss, TEXT("thalassor")) && Id == TEXT("sluice_break"))
	{
		// It rams the east sluices: the three closed gates burst; the surge crosses the flat; the Pound floods.
		if (AWROceanState* OS = AWROceanState::Get(Boss))
		{
			OS->bSluicesBurst = true;
			OS->SluicesBurstAt = Boss->GetWorld()->GetGameState() ? Boss->GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : 0.0;
		}
		const FVector C = PoundCentre();
		WRFx::Broadcast(Boss, TEXT("ocean.bore"), C + FVector(PoundInnerR(), 0, 0), FRotator(0.f, 180.f, 0.f), 1.5f, TEXT("ocean.bore_roar"));
		// The surge runs across the flat from the burst gates, west, at surgeSpeed (cookpass-combat 3.10): it takes you
		// where its front reaches you, so you can see it coming and climb a tower or jump it.
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		AWRStrikeFront::Line(Boss, C + FVector(PoundInnerR(), 0, 0), C - FVector(PoundInnerR(), 0, 0), ObT(Boss, TEXT("surgeSpeed"), 600.f), PoundInnerR() * 0.6f,
			[Weak, C](AWRCharacterBase* V)
			{
				AWRBoss* B = Weak.Get();
				if (!B || V == B || UWROceanBody::Get(V) || FVector::Dist2D(V->GetActorLocation(), C) > PoundInnerR()) return;
				// On a tower or the wall (high): over it. In the air (jumped): over it.
				const float Feet = V->GetActorLocation().Z - V->GetSimpleCollisionHalfHeight();
				if (Feet > WRTide::MeanZ() + 300.f || V->GetCharacterMovement()->IsFalling()) return;
				ObHit(B, V, ObT(B, TEXT("surgeDamage"), 60.f), V->GetActorLocation() + FVector(300.f, 0.f, 0.f));
				if (V->Status) V->Status->Stagger(1.2f);
				WRSkyAir::Push(V, FVector(-800.f, 0.f, 100.f), 0.5f, TEXT("surge"), B);
			}, TEXT("tide.high_tide"), TEXT("ocean.bore_roar"));
		SetFlag(Boss, WROceanFlag::Flare, false);
		return false;   // the row's own ring still lands (the scream's shockwave)
	}
	return false;
}

bool FWROceanBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = ObNow(Boss);
	if (Is(Boss, TEXT("siren_eel")) && Id == TEXT("siren_song") && T < S.SongUntil)
	{
		// The Lure: players within 20 m who can hear (no wax in their ears) drawn to her at 250 cm/s.
		if (FMath::Fmod(float(T), 0.25f) < DeltaSeconds)
		{
			for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > ObT(Boss, TEXT("songLureReach"), 2000.f)) continue;
				if (UWRInventoryComponent::GearStatFor(*It, TEXT("passSirenSong")) > 0.f) continue;
				WRSkyAir::Push(*It, (Boss->GetActorLocation() - It->GetActorLocation()).GetSafeNormal2D() * ObT(Boss, TEXT("songLure"), 250.f), 0.25f, TEXT("lure"), Boss);
			}
		}
		return false;
	}
	if (Is(Boss, TEXT("thalassor")) && Id == TEXT("undertow"))
	{
		// Swimmers within 15 m of where it circles pulled to the centre (swim sideways out of it).
		if (FMath::Fmod(float(T), 0.25f) < DeltaSeconds)
		{
			for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
			{
				AWRCharacterBase* V = *It;
				if (V == Boss || !V->IsAlive() || UWROceanBody::Get(V) || !V->GetCharacterMovement()->IsSwimming()) continue;
				const FVector D = Boss->AttackPoint - V->GetActorLocation();
				if (D.Size2D() > 1500.f || D.Size2D() < 150.f) continue;
				WRSkyAir::Push(V, D.GetSafeNormal2D() * ObT(Boss, TEXT("undertowPull"), 250.f), 0.25f, TEXT("undertow"), Boss);
			}
		}
		return false;
	}
	if (Is(Boss, TEXT("thalassor")) && Id == TEXT("bubble_net"))
	{
		// Inside the ring the water won't hold you up: swimmers sink, waders slow.
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Boss || !V->IsAlive() || FVector::Dist2D(V->GetActorLocation(), Boss->AttackPoint) > Attack.Size) continue;
			if (V->GetCharacterMovement()->IsSwimming()) V->GetCharacterMovement()->Velocity.Z = FMath::Min(V->GetCharacterMovement()->Velocity.Z, -ObT(Boss, TEXT("bubbleNetSink"), 200.f));
			else if (V->Status && FMath::Fmod(float(T), 0.5f) < DeltaSeconds) V->Status->ApplyStatus(WRTags::Status_Slowed, 0.6f);
		}
		return false;
	}
	return false;
}

void FWROceanBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = ObNow(Boss);
	SetFlag(Boss, WROceanFlag::Gape, false);
	SetFlag(Boss, WROceanFlag::Cocked, false);
	if (Is(Boss, TEXT("old_barnacle")) && Id == TEXT("hull_slam")) SetFlag(Boss, WROceanFlag::Guard, false);
	if (Is(Boss, TEXT("siren_eel")) && Id == TEXT("siren_song")) { SetFlag(Boss, WROceanFlag::Flare, false); S.bSongWindup = false; }
	if (Is(Boss, TEXT("reef_tyrant")) && Id == TEXT("breach"))
	{
		// It crashes down: in under 2 m of water it beaches itself.
		const float Depth = WRTide::SurfaceAt(Boss, Boss->GetActorLocation()) - WRLand::GroundZAt(Boss->GetActorLocation(), -100000.f);
		if (Depth < ObT(Boss, TEXT("beachDepth"), 200.f))
		{
			S.BeachedUntil = T + ObT(Boss, TEXT("beachSeconds"), 8.f);
			Boss->StanceOpenUntil = S.BeachedUntil;
			SetFlag(Boss, WROceanFlag::Flipped, true);
		}
	}
	if (Is(Boss, TEXT("thalassor")))
	{
		// A lunge that ends 10 m out from the pool, or into a tower, beaches it 5 s.
		if (Id == TEXT("head_ram") || Id == TEXT("swallow_lunge"))
		{
			const FVector C = PoundCentre();
			bool bBeach = FVector::Dist2D(Boss->GetActorLocation(), C) > PoundPoolR() + ObT(Boss, TEXT("beachFromPool"), 1000.f);
			for (const TWeakObjectPtr<AWROceanBossProp>& W : AWROceanBossProp::All())
			{
				if (W.IsValid() && W->Kind == TEXT("tower") && W->IsLive() && FVector::Dist2D(W->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetSimpleCollisionRadius() + 350.f) bBeach = true;
			}
			if (bBeach && Boss->Phase <= 1 && UWROceanArenas::FloodLevel(Boss) < -1.0e8f)
			{
				S.ThalBeachedUntil = T + ObT(Boss, TEXT("beachSeconds"), 5.f);
				Boss->StanceOpenUntil = S.ThalBeachedUntil;
				SetFlag(Boss, WROceanFlag::Flipped, true);
				WRFx::Broadcast(Boss, TEXT("ocean.splash"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("boss.thalassor.beached"));
				UE_LOG(LogWildreach, Log, TEXT("OCEAN Thalassor is beached"));
			}
		}
		// A breach along its line breaks the towers there.
		if (Id == TEXT("breach"))
		{
			for (const TWeakObjectPtr<AWROceanBossProp>& W : AWROceanBossProp::All())
			{
				AWROceanBossProp* P = W.Get();
				if (!P || P->Kind != TEXT("tower") || !P->IsLive()) continue;
				const FVector Q = FMath::ClosestPointOnSegment(P->GetActorLocation(), Boss->StrikeFrom, Boss->GetActorLocation() + Boss->GetActorForwardVector() * 1500.f);
				if (FVector::Dist2D(Q, P->GetActorLocation()) < 600.f)
				{
					P->SetState(1);
					WRFx::Broadcast(Boss, TEXT("hit.dust"), P->GetActorLocation() + FVector(0, 0, 250.f), FRotator::ZeroRotator, 5.f, TEXT("boss.thalassor.tower"));
				}
			}
		}
		if (Id == TEXT("sluice_break")) SetFlag(Boss, WROceanFlag::Flare, false);
	}
}

void FWROceanBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority() || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (!bLanded) return;
	// The grabs: in the claw (thrown 8 m), under the water (the jaw, the drag), swallowed (spat 10 m).
	if (Is(Boss, TEXT("old_barnacle")) && AttackId == TEXT("pinch_hold"))
		FWROceanAI::StartHold(Boss, Victim, TEXT("pinch"), ObT(Boss, TEXT("pinchSeconds"), 2.f), ObT(Boss, TEXT("pinchDps"), 18.f), 0.f, 1.0e9f, ObT(Boss, TEXT("pinchThrow"), 800.f));
	else if (Is(Boss, TEXT("siren_eel")) && AttackId == TEXT("second_jaw"))
		FWROceanAI::StartHold(Boss, Victim, TEXT("under"), ObT(Boss, TEXT("jawSeconds"), 2.f), ObT(Boss, TEXT("jawDps"), 15.f), ObT(Boss, TEXT("jawDepth"), 400.f), 1.0e9f, 0.f);
	else if (Is(Boss, TEXT("reef_tyrant")) && AttackId == TEXT("thrash_drag"))
		FWROceanAI::StartHold(Boss, Victim, TEXT("drag"), ObT(Boss, TEXT("dragSeconds"), 3.f), ObT(Boss, TEXT("dragDps"), 18.f), ObT(Boss, TEXT("dragDepth"), 1500.f), 1.0e9f, 0.f);
	else if (Is(Boss, TEXT("thalassor")) && AttackId == TEXT("swallow_lunge"))
		FWROceanAI::StartHold(Boss, Victim, TEXT("swallow"), ObT(Boss, TEXT("swallowSeconds"), 3.f), ObT(Boss, TEXT("swallowDps"), 20.f), 0.f, ObT(Boss, TEXT("swallowFree"), 150.f), ObT(Boss, TEXT("swallowSpit"), 1000.f));
}

int32 FWROceanBoss::PartAt(const AWRBoss* Boss, const FVector& P, float Reach)
{
	if (!Boss) return -1;
	const FVector Fwd = Boss->GetActorForwardVector(), C = Boss->GetActorLocation();
	const FVector Parts[4] = { C + Fwd * 700.f, C + Fwd * 1150.f + FVector(0, 0, 250.f), C - Fwd * 1500.f, C - Fwd * 4100.f };
	int32 Best = -1;
	float BestD = Reach;
	for (int32 i = 0; i < 4; ++i) { const float D = FVector::Dist(P, Parts[i]); if (D < BestD) { BestD = D; Best = i; } }
	return Best;
}

void FWROceanBoss::AddBeam(AWRBoss* Boss, int32 Part, float Seconds)
{
	if (!Is(Boss, TEXT("thalassor")) || Part < 0 || Part > 3 || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const double T = ObNow(Boss);
	const float K = T < S.ThalBeachedUntil ? 2.f : Part == 1 ? 2.f : 1.f;   // beached: double; the blowhole dries x2
	// The Maelstrom: 3 s of beam on its body breaks the vortex 8 s.
	if (Boss->Phase >= 3 && T >= S.MaelstromOffUntil)
	{
		S.MaelstromBeam += Seconds * K;
		if (S.MaelstromBeam >= ObT(Boss, TEXT("maelstromBreakBeam"), 3.f))
		{
			S.MaelstromBeam = 0.f;
			S.MaelstromOffUntil = T + ObT(Boss, TEXT("maelstromBreakSeconds"), 8.f);
			WRFx::Broadcast(Boss, TEXT("ocean.sun_crack"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("boss.thalassor.dried"));
			UE_LOG(LogWildreach, Log, TEXT("OCEAN the sun breaks the Maelstrom"));
		}
	}
	if (S.DriedThisPhase.Contains(Part) || T < S.DriedUntil[Part]) return;
	S.Beam[Part] += Seconds * K;
	if (S.Beam[Part] >= ObT(Boss, TEXT("sunDry"), 2.f))
	{
		S.Beam[Part] = 0.f;
		S.DriedUntil[Part] = T + ObT(Boss, TEXT("sunDrySeconds"), 20.f);
		S.DriedThisPhase.Add(Part);
		const FVector Fwd = Boss->GetActorForwardVector(), C = Boss->GetActorLocation();
		const FVector At = Part == 0 ? C + Fwd * 700.f : Part == 1 ? C + Fwd * 1150.f + FVector(0, 0, 250.f) : Part == 2 ? C - Fwd * 1500.f : C - Fwd * 4100.f;
		WRFx::Broadcast(Boss, TEXT("ocean.sun_crack"), At, FRotator::ZeroRotator, 2.f, TEXT("boss.thalassor.dried"));
		UE_LOG(LogWildreach, Log, TEXT("OCEAN Thalassor's part %d is sun-dried"), Part);
	}
}

void FWROceanBoss::SunBurst(UWorld* World, const FVector& At, float Seconds)
{
	AWRBoss* Thal = World ? FindBoss(World, TEXT("thalassor")) : nullptr;
	if (!Thal) return;
	FWROceanBoss::AddBeam(Thal, PartAt(Thal, At, 800.f), Seconds);
}

float FWROceanBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsOceanBoss(Boss)) return Damage;
	FState& S = St(Boss);
	const double T = ObNow(Boss);
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : Hit.Origin;
	if (Is(Boss, TEXT("old_barnacle")))
	{
		const bool bBlunt = Hit.PhysType == 2;
		const bool bFire = Hit.Element == WRTags::Element_Fire;
		if (T < S.FortressUntil && !bBlunt && !bFire) Damage *= ObT(Boss, TEXT("fortressSlashPierce"), 0.2f);
		if (S.bNaked || S.bSeeking) return Damage * ObT(Boss, TEXT("nakedAbdomen"), 2.f);
		if (S.Shell > 0.f)
		{
			// The shell takes it (blunt x2), a plank torn off each 300 where you can see it; fire on the tarred timber.
			const float ToShell = Damage * (bBlunt ? ObT(Boss, TEXT("hullBluntMult"), 2.f) : 1.f);
			S.Shell -= ToShell;
			S.ShellTorn += ToShell;
			if (S.ShellTorn >= ObT(Boss, TEXT("plankEvery"), 300.f))
			{
				S.ShellTorn = 0.f;
				WRFx::Broadcast(Boss, TEXT("ocean.plank"), Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.5f, TEXT("boss.old_barnacle.plank"));
			}
			if (bFire && S.ShellKind == TEXT("hull") && S.FireOnShellAt < 0.0)
			{
				S.FireOnShellAt = T;
				WRFx::Broadcast(Boss, TEXT("fire.impact"), Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 2.f, TEXT("boss.old_barnacle.burning"), 3.f);
			}
			return Damage * ObT(Boss, TEXT("hullTaken"), 0.2f);
		}
		return Damage;
	}
	if (Is(Boss, TEXT("reef_tyrant")) && T < S.BeachedUntil) return Damage * ObT(Boss, TEXT("beachTaken"), 1.3f);
	if (Is(Boss, TEXT("thalassor")))
	{
		// Soak on it barely matters (it is the sea); the Sun hurts it; a dried part x1.5; beached x1.3; the open blowhole x1.5.
		if (Hit.PathId == TEXT("sun")) Damage *= ObT(Boss, TEXT("sunWeakMult"), 1.5f);
		const int32 Part = PartAt(Boss, At, 1200.f);
		if (Part >= 0 && T < S.DriedUntil[Part]) Damage *= ObT(Boss, TEXT("sunDryTaken"), 1.5f);
		if (T < S.ThalBeachedUntil) Damage *= ObT(Boss, TEXT("beachTaken"), 1.3f);
		if (Part == 1 && T < S.BreathUntil) Damage *= ObT(Boss, TEXT("blowholeTaken"), 1.5f);
		return Damage;
	}
	return Damage;
}

bool FWROceanBoss::OwnWeakness(const AWRBoss* Boss)
{
	return Is(Boss, TEXT("thalassor"));   // the Sun's x1.5 is here, not the generic element weakness
}

void FWROceanBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const double T = ObNow(Boss);
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : Hit.Origin;
	// A held survivor struck free by allies' blows (the swallow's 150 to the head).
	if (UWROceanBody* Bd = Body(Boss); Bd && Bd->HeldBody.IsValid() && Hit.Instigator != Bd->HeldBody.Get()) Bd->FreeDamage += Damage;
	if (Is(Boss, TEXT("siren_eel")) && S.bSongWindup)
	{
		// 150 to her crest in the windup breaks the song and staggers her 3 s.
		if (At.Z > Boss->GetActorLocation().Z) S.CrestDamage += Damage;
		if (S.CrestDamage >= ObT(Boss, TEXT("crestBreak"), 150.f))
		{
			S.bSongWindup = false;
			SetFlag(Boss, WROceanFlag::Flare, false);
			Boss->Current = INDEX_NONE;
			Boss->StrikeState = AWRBoss::EStrike::None;
			if (Boss->Status) Boss->Status->Stagger(ObT(Boss, TEXT("crestStagger"), 3.f));
			WRFx::Broadcast(Boss, TEXT("hit.spark"), Boss->GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 2.f, TEXT("boss.siren_eel.broken"));
			UE_LOG(LogWildreach, Log, TEXT("OCEAN the Siren's song broken at the crest"));
		}
	}
	if (Is(Boss, TEXT("thalassor")))
	{
		// The Sun path's hits count as 1 s of beam; the blowhole breaks at 1,000 (no more Spout volley this phase).
		if (Hit.PathId == TEXT("sun")) AddBeam(Boss, PartAt(Boss, At, 1200.f), 1.f);
		if (PartAt(Boss, At, 1200.f) == 1 && T < S.BreathUntil && !S.bBlowholeBroken)
		{
			S.BlowholeHealth -= Damage;
			if (S.BlowholeHealth <= 0.f)
			{
				S.bBlowholeBroken = true;
				WRFx::Broadcast(Boss, TEXT("hit.blood"), At, FRotator::ZeroRotator, 3.f, TEXT("boss.thalassor.blowhole"));
			}
		}
	}
}

void FWROceanBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	// One crack per part a phase; the blowhole grows back.
	S.DriedThisPhase.Reset();
	S.bBlowholeBroken = false;
	S.BlowholeHealth = ObT(Boss, TEXT("blowholeHealth"), 1000.f);
	if (Is(Boss, TEXT("thalassor")) && NewPhase >= 3) WRSound::PlayAt(Boss, TEXT("boss.thalassor.maelstrom"), Boss->GetActorLocation());
}

bool FWROceanBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!Is(Boss, TEXT("thalassor"))) return false;
	FVector Rb;
	if (!FWROceanData::Get().ArenaPoint(TEXT("thalassor"), TEXT("runback"), Rb)) return false;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Tr(FRotator(0.f, (PoundCentre() - Rb).Rotation().Yaw, 0.f), Rb);
	if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActorDeferred<AWRRunbackStone>(AWRRunbackStone::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Stone->BossId = Boss->CreatureId;
		Stone->FinishSpawning(Tr);
		Boss->Runback = Stone;
	}
	return true;
}

FName FWROceanBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Is(Boss, TEXT("thalassor"))) return TEXT("leviathan_pearl");
	return Fallback;
}

void FWROceanBoss::Died(AWRBoss* Boss)
{
	if (!IsOceanBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	for (const TPair<TWeakObjectPtr<AWRCreature>, double>& C : S.Charmed) if (C.Key.IsValid()) C.Key->SetAggroTarget(nullptr);
	S.Charmed.Reset();
	// Hae-won's sixth: the Tyrant beaten after it was tied off counts for everyone in the fight.
	if (Is(Boss, TEXT("reef_tyrant")) && S.bEverTethered)
	{
		UWRWorldSystems* WS = Boss->GetWorld()->GetSubsystem<UWRWorldSystems>();
		for (FConstPlayerControllerIterator It = Boss->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AWRPlayerState>() : nullptr;
			if (WS && PS && Boss->Ledger.Contains(PS->GetPlayerName())) WS->QuestEvent(PS, TEXT("kill.tethered"), Boss->CreatureId);
		}
	}
	if (Is(Boss, TEXT("thalassor"))) if (UWROceanArenas* A = UWROceanArenas::Get(Boss)) { FTimerHandle H; TWeakObjectPtr<UWROceanArenas> WA(A); Boss->GetWorldTimerManager().SetTimer(H, [WA]() { if (WA.IsValid()) WA->ResetPound(); }, 60.f, false); }
}

bool FWROceanBoss::HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire)
{
	AWROceanBossProp* P = Cast<AWROceanBossProp>(Thing);
	if (!P || !P->HasAuthority() || Cast<AWRBoss>(By)) return false;
	(void)Comp;
	return P->TakeHit(By, Damage, bFire, false);
}

// ================================================================================================ the arenas

UWROceanArenas* UWROceanArenas::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWROceanArenas>() : nullptr;
}

bool UWROceanArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld();
}

TStatId UWROceanArenas::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWROceanArenas, STATGROUP_Tickables);
}

bool UWROceanArenas::IsTickable() const
{
	return GetWorld() && WROcean::Live();
}

float UWROceanArenas::FloodLevel(const UObject* WorldContext)
{
	const AWROceanState* S = AWROceanState::Get(WorldContext);
	if (!S || !S->bSluicesBurst) return -1.0e9f;
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const double Now = W && W->GetGameState() ? W->GetGameState()->GetServerWorldTimeSeconds() : 0.0;
	const float Secs = FMath::Max(1.f, WRTune(TEXT("bosses.thalassor.floodSeconds"), 10.f));
	const float A = FMath::Clamp(float((Now - S->SluicesBurstAt) / Secs), 0.f, 1.f);
	const float Flat = FWROceanData::Get().ArenaNum(TEXT("thalassor"), TEXT("flat"), -0.1f) * 100.f;
	const float Base = FMath::Max(WRTide::MeanZ(), Flat + FWROceanData::Get().ArenaNum(TEXT("thalassor"), TEXT("waterP1"), 0.65f) * 100.f);
	return Base + A * WRTune(TEXT("bosses.thalassor.floodRise"), 400.f);
}

void UWROceanArenas::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	StepFlood();
	if (World->GetNetMode() == NM_Client) return;
	PlaceClock += DeltaTime;
	if (PlaceClock < 3.f) return;
	PlaceClock = 0.f;
	for (const TCHAR* Id : { TEXT("old_barnacle"), TEXT("siren_eel"), TEXT("reef_tyrant"), TEXT("thalassor") })
	{
		const FName B(Id);
		if (!Placed.Contains(B) && Place(B)) Placed.Add(B);
	}
}

bool UWROceanArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	const FWROceanData& D = FWROceanData::Get();
	if (!FWRCreatureRegistry::Get().Find(BossId)) return true;
	// Only once someone's in the Ocean.
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && WROcean::Contains(It->GetActorLocation());
	if (!bNear) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It) if (It->CreatureId == BossId) return true;
	FVector Start;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (BossId == TEXT("old_barnacle"))
	{
		FVector A3;
		if (!D.ArenaPoint(TEXT("old_barnacle"), TEXT("at"), A3)) return true;
		Start = FVector(A3.X, A3.Y, WRLand::GroundZAt(A3, WRTide::MeanZ()) + 250.f);
	}
	else if (BossId == TEXT("siren_eel"))
	{
		const FWROceanData::FCave* Cv = D.FindCave(TEXT("sirens_hollow"));
		if (!Cv) return true;
		Start = FVector(Cv->At.X, Cv->At.Y, WRTide::SurfaceAt(World, Cv->At) - 400.f);
	}
	else if (BossId == TEXT("reef_tyrant"))
	{
		const TArray<FVector> Route = D.ArenaPoints(TEXT("reef_tyrant"), TEXT("route"));
		if (Route.Num() == 0) return true;
		Start = FVector(Route[0].X, Route[0].Y, WRTide::SurfaceAt(World, Route[0]) - 600.f);
	}
	else if (BossId == TEXT("thalassor"))
	{
		const FVector C = PoundCentre();
		Start = FVector(C.X, C.Y, WRTide::SurfaceAt(World, C) - 400.f);
		if (!bPoundBuilt)
		{
			bPoundBuilt = true;
			// The flat's eight stilt-tower stumps (they break), the three Sunwatch mirrors on the wall, the Tide altar.
			int32 i = 0;
			for (const FVector& Tw : D.ArenaPoints(TEXT("thalassor"), TEXT("towers")))
			{
				const FVector G(Tw.X, Tw.Y, WRLand::GroundZAt(Tw, C.Z));
				if (AWROceanBossProp* P = World->SpawnActorDeferred<AWROceanBossProp>(AWROceanBossProp::StaticClass(), FTransform(G), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					P->Kind = TEXT("tower");
					P->Index = i++;
					P->FinishSpawning(FTransform(G));
				}
			}
			// The mirrors stand on the plinths the wall's scenery built by its three stairs (WROceanScenery Pound: the plinth
			// is R + half the wall's thickness out along the stair's bearing, 5 m along the walkway, its drum 90 cm tall).
			const float R = D.PoundRadius();
			const float WallTop = D.ArenaNum(TEXT("thalassor"), TEXT("wallTop"), 30.f) * 100.f;
			for (const float Bearing : { -90.f, 180.f, 90.f })
			{
				const FVector Radial = FRotator(0.f, Bearing, 0.f).Vector(), Tangent = FRotator(0.f, Bearing + 90.f, 0.f).Vector();
				const FVector At = FVector(C.X, C.Y, C.Z + WallTop + 90.f) + Radial * (R + 250.f) - Tangent * 500.f;
				World->SpawnActor<AWRSunMirror>(AWRSunMirror::StaticClass(), At, FRotator(0.f, Bearing + 180.f, 0.f), SP);
			}
			FVector Al;
			if (D.ArenaPoint(TEXT("thalassor"), TEXT("altar"), Al))
			{
				const FTransform T(FRotator(0.f, (C - Al).Rotation().Yaw, 0.f), Al);
				if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					Altar->PathId = TEXT("tide");
					Altar->FinishSpawning(T);
				}
			}
		}
	}
	else return true;
	const FTransform Where(FRotator::ZeroRotator, Start);
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = BossId == TEXT("old_barnacle");
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("OCEAN arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}

void UWROceanArenas::SpawnSpares(AWRBoss* Barnacle)
{
	// The two spare shells on the cove's beach, in plain view from its entrance (each once: they come back with the fight).
	FState& S = St(Barnacle);
	S.Spares.RemoveAll([](const TWeakObjectPtr<AWROceanBossProp>& W) { return !W.IsValid(); });
	if (S.Spares.Num() > 0) return;
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("ocean.json"));
	const TSharedPtr<FJsonObject>* Arenas = nullptr;
	const TSharedPtr<FJsonObject>* Ob = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Spares = nullptr;
	if (!File.IsValid() || !File->TryGetObjectField(TEXT("arenas"), Arenas) || !(*Arenas)->TryGetObjectField(TEXT("old_barnacle"), Ob) || !(*Ob)->TryGetArrayField(TEXT("spares"), Spares)) return;
	FVector A3;
	if (!FWROceanData::Get().ArenaPoint(TEXT("old_barnacle"), TEXT("at"), A3)) return;
	for (const TSharedPtr<FJsonValue>& V : *Spares)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FName Kind(*O->GetStringField(TEXT("kind")));
		FVector At = A3 + FVector(O->GetNumberField(TEXT("dx")), O->GetNumberField(TEXT("dy")), 0.0) * 100.0;
		At.Z = WRLand::GroundZAt(At, A3.Z);
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At);
		if (AWROceanBossProp* P = GetWorld()->SpawnActorDeferred<AWROceanBossProp>(AWROceanBossProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			P->Kind = Kind;
			P->MaxHealth = P->Health = Kind == TEXT("bell") ? ObT(Barnacle, TEXT("bellHealth"), 800.f) : ObT(Barnacle, TEXT("carapaceHealth"), 400.f);
			P->FinishSpawning(T);
			S.Spares.Add(P);
		}
	}
}

void UWROceanArenas::ResetPound()
{
	if (AWROceanState* S = AWROceanState::Get(this)) { S->bSluicesBurst = false; S->SluicesBurstAt = -100.0; }
	for (const TWeakObjectPtr<AWROceanBossProp>& W : AWROceanBossProp::All()) if (W.IsValid() && W->Kind == TEXT("tower")) W->SetState(0);
	for (const TWeakObjectPtr<AWRSunMirror>& M : AWRSunMirror::All()) if (M.IsValid()) { M->Operator.Empty(); M->OperatorPawn = nullptr; }
}

void UWROceanArenas::StepFlood()
{
	// Every machine: the Pound's flood water from the replicated burst time (a water volume over the tide's and its
	// surface drawn with the sea's own material).
	const float Level = FloodLevel(this);
	UWorld* World = GetWorld();
	if (Level < -1.0e8f)
	{
		if (Flood) { Flood->Destroy(); Flood = nullptr; }
		if (FloodLook) { FloodLook->Destroy(); FloodLook = nullptr; }
		return;
	}
	const FVector C = PoundCentre();
	const float R = PoundInnerR();
	if (!Flood)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Flood = World->SpawnActor<AWRTideWater>(AWRTideWater::StaticClass(), FTransform(C), SP);
		if (Flood) Flood->Shape(FBox2D(FVector2D(C) - FVector2D(R, R), FVector2D(C) + FVector2D(R, R)), C.Z - 1500.f, Level, true, int32(WRTune(TEXT("oceanHazards.pocketPriority"), 6.f)) + 1);
		if (World->GetNetMode() != NM_DedicatedServer)
		{
			FloodLook = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(C), SP);
			if (FloodLook)
			{
				UProceduralMeshComponent* Disc = NewObject<UProceduralMeshComponent>(FloodLook, TEXT("Surface"));
				FloodLook->SetRootComponent(Disc);
				Disc->RegisterComponent();
				Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				TArray<FVector> V; TArray<int32> Tr; TArray<FVector> N; TArray<FVector2D> UV;
				V.Add(FVector::ZeroVector); N.Add(FVector::UpVector); UV.Add(FVector2D(0.5f, 0.5f));
				const int32 Seg = 64;
				for (int32 k = 0; k < Seg; ++k) { const float A = 2.f * PI * k / Seg; V.Add(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f)); N.Add(FVector::UpVector); UV.Add(FVector2D(FMath::Cos(A) * R / 1000.f, FMath::Sin(A) * R / 1000.f)); }
				for (int32 k = 0; k < Seg; ++k) { const int32 A = 1 + k, B = 1 + (k + 1) % Seg; Tr.Append({ 0, B, A, 0, A, B }); }
				Disc->CreateMeshSection(0, V, Tr, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
				if (UMaterialInterface* Sea = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("sea.material"), TEXT("/Game/Sea/MI_Ocean.MI_Ocean")))) Disc->SetMaterial(0, Sea);
			}
		}
	}
	if (Flood) Flood->SetLevel(Level);
	if (FloodLook) FloodLook->SetActorLocation(FVector(C.X, C.Y, Level));
}

bool UWROceanArenas::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	const FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	for (const TWeakObjectPtr<AWRSunMirror>& W : AWRSunMirror::All())
	{
		const AWRSunMirror* M = W.Get();
		if (!M || FVector::Dist2D(M->GetActorLocation(), Me->GetActorLocation()) > WRTune(TEXT("bosses.thalassor.mirrorReach"), 300.f)) continue;
		OutTarget = TEXT("A Sunwatch mirror");
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (M->Operator == Name) OutVerb = Inv && Inv->CountOf(TEXT("sunstone")) > 0 && M->SunstoneLeft <= 0.f ? TEXT("Load a sunstone (or step away to let go)") : TEXT("Let go");
		else if (!M->Operator.IsEmpty()) OutVerb = FString::Printf(TEXT("%s has it"), *M->Operator);
		else OutVerb = TEXT("Take the mirror (aim with the camera)");
		return true;
	}
	return false;
}

bool UWROceanArenas::ClientInteract(AWRPlayerController* PC)
{
	FString T, V;
	if (!Prompt(PC, T, V)) return false;
	PC->SendCommand(TEXT("world ocean mirror"));
	return true;
}

FString UWROceanArenas::Command(AWRPlayerController* PC)
{
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return FString();
	const FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	for (const TWeakObjectPtr<AWRSunMirror>& W : AWRSunMirror::All())
	{
		AWRSunMirror* M = W.Get();
		if (!M || FVector::Dist2D(M->GetActorLocation(), Me->GetActorLocation()) > WRTune(TEXT("bosses.thalassor.mirrorReach"), 300.f)) continue;
		if (M->Operator == Name)
		{
			UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
			if (Inv && Inv->CountOf(TEXT("sunstone")) > 0 && M->SunstoneLeft <= 0.f)
			{
				Inv->RemoveItem(TEXT("sunstone"), 1);
				M->SunstoneLeft = WRTune(TEXT("bosses.thalassor.sunstoneSeconds"), 30.f);
				return TEXT("The sunstone in the cup: 30 s of daylight, day or night");
			}
			M->Operator.Empty();
			M->OperatorPawn = nullptr;
			return TEXT("You let the mirror go");
		}
		if (!M->Operator.IsEmpty()) return FString::Printf(TEXT("%s has this mirror"), *M->Operator);
		M->Operator = Name;
		M->OperatorPawn = Me;
		float Sun = 0.f;
		Daylight(M, Sun, FindBoss(M->GetWorld(), TEXT("thalassor")));
		return Sun > 0.f ? TEXT("The mirror's yours: the sun's beam goes where you look") : M->SunstoneLeft > 0.f ? TEXT("The mirror's yours: the sunstone's light goes where you look")
			: TEXT("The mirror's yours, but there's no sun to throw (load a sunstone: E again)");
	}
	return TEXT("No mirror here");
}
