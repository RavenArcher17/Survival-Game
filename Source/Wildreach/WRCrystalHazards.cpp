// The Crystal caves' hazards and machines (see WRCrystalHazards.h; cooks/biome-crystal.md 2.5, 2.8, 2.10).
//
// Everything that moves is a pure function of the server's clock and a replicated phase (build-biome-crystal 2.10): the
// chandelier's fall is a gravity curve from PhaseAt, its re-hang a 20 s ease up its chain; the Vault door rolls 3 s on
// its rail from OpenAt and back from CloseAt. Nothing snaps on a tick.

#include "WRCrystalHazards.h"
#include "WRCrystalThings.h"
#include "WRCrystalWorld.h"
#include "WRCrystalCaves.h"
#include "WRCrystalTypes.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace WRCrystalHazImpl
{
	float Tn(const TCHAR* Key, float Def) { return WRTune(Key, Def); }
	float Now(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	void Tell(const AActor* Pawn, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Pawn);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	UMaterialInstanceDynamic* GlowMID(UObject* Outer, const FLinearColor& Col, float Rest)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow"));
		if (!Mat) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, Outer);
		M->SetVectorParameterValue(TEXT("GlowColor"), Col);
		M->SetScalarParameterValue(TEXT("GlowScale"), Tn(TEXT("crystalWorld.glow.scale"), 6.f));
		M->SetScalarParameterValue(TEXT("Rest"), Rest);
		return M;
	}
	UProceduralMeshComponent* Mesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner, Name);
		P->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		P->RegisterComponent();
		return P;
	}
	/** A flat quad facing Up (two triangles, both faces). */
	void Quad(FWRToolMesh& M, const FVector& C, const FVector& U, const FVector& V, const FColor& Col)
	{
		const FVector2D Z = FVector2D::ZeroVector;
		const FVector A = C - U - V, B = C + U - V, D = C + U + V, E = C - U + V;
		M.Tri(A, B, D, Col, Z, Z, Z); M.Tri(A, D, E, Col, Z, Z, Z);
		M.Tri(A, D, B, Col, Z, Z, Z); M.Tri(A, E, D, Col, Z, Z, Z);
	}
	/** A hexafoil (the Wardcarvers' mark, crystal 2.20): six circles on a seventh's rim, in a plane (Right, Up). */
	void Hexafoil(FWRToolMesh& M, const FVector& Centre, const FVector& Right, const FVector& Up, float R, float Thick, const FLinearColor& Col, float Rough)
	{
		const auto Ring = [&](const FVector& C, float Rr)
		{
			TArray<FVector> P;
			for (int32 k = 0; k < 24; ++k) { const float A = k * 2.f * PI / 24.f; P.Add(C + (Right * FMath::Cos(A) + Up * FMath::Sin(A)) * Rr); }
			M.Tube(P, Thick, 5, Col, Rough, true);
		};
		Ring(Centre, R * 2.f);
		for (int32 k = 0; k < 6; ++k) { const float A = k * PI / 3.f; Ring(Centre + (Right * FMath::Cos(A) + Up * FMath::Sin(A)) * R, R); }
	}
	bool InSettingVoid(const AActor* A)
	{
		const AWRCreature* C = Cast<AWRCreature>(A);
		const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
		return D && (D->Biome == TEXT("wasteland") || D->Biome == TEXT("void"));
	}
	bool CrystalBorn(const AActor* A)
	{
		const AWRCreature* C = Cast<AWRCreature>(A);
		const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
		return D && D->Biome == WRCrystal::Biome() && !C->IsTamed();
	}
	bool ResonanceOn(const UObject* Ctx)
	{
		const AWRWorldState* WS = AWRWorldState::Get(Ctx);
		const FWRWorldEventRep* E = WS ? WS->Event(TEXT("resonance")) : nullptr;
		return E && E->Stage == 1 && Now(Ctx) < E->EndAt;
	}
}
using namespace WRCrystalHazImpl;

// ================================================================================================ AWRWardField

AWRWardField::AWRWardField()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRWardField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWardField, Ward);
	DOREPLIFETIME(AWRWardField, DownUntil);
	DOREPLIFETIME(AWRWardField, bPattern);
}

bool AWRWardField::IsLive() const
{
	return bPattern && Now(this) >= DownUntil;
}

void AWRWardField::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

bool AWRWardField::IsAttuned(const AActor* Who, bool& bHalf)
{
	bHalf = false;
	if (CrystalBorn(Who)) return true;
	if (const UWRPathComponent* P = UWRPathComponent::FindFor(Who); P && P->IsActivePath(TEXT("arcane"))) return true;
	if (UWRInventoryComponent::GearStatFor(Who, TEXT("passWards")) > 0.f || UWRInventoryComponent::GearStatFor(Who, TEXT("wardNoThrow")) > 0.f) return true;
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who); C && C->HasTag(WRTags::Status_Attuned)) bHalf = true;
	return false;
}

void AWRWardField::BuildLook()
{
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.Wards.IsValidIndex(Ward)) return;
	const FWRCrystalData::FWard& W = D.Wards[Ward];
	// Local frame: X the way through (the sheet's normal), Y across, Z up; the actor sits at the sheet's floor centre.
	FWRToolMesh F;
	const float Hw = W.W * 0.5f;
	const FLinearColor Violet(FColor(150, 110, 230));
	TArray<FVector> Edge = { FVector(0.f, -Hw, 0.f), FVector(0.f, -Hw, W.H), FVector(0.f, Hw, W.H), FVector(0.f, Hw, 0.f) };
	TArray<FVector> Line;
	for (int32 s = 0; s + 1 < Edge.Num(); ++s) for (int32 k = 0; k <= 12; ++k) Line.Add(FMath::Lerp(Edge[s], Edge[s + 1], k / 12.f));
	F.Tube(Line, 2.5f, 5, Violet, 0.6f, false);
	// The hexafoil carved over the opening (crystal 2.8: it glows with the tell), facing both ways.
	Hexafoil(F, FVector(0.f, 0.f, W.H + 45.f), FVector(0.f, 1.f, 0.f), FVector::UpVector, 16.f, 2.2f, Violet, 1.f);
	Frame = Mesh(this, nullptr, TEXT("WardFrame"));
	F.ApplyTo(Frame);
	FrameGlow = GlowMID(this, FLinearColor(0.62f, 0.42f, 1.f), 0.05f);
	if (FrameGlow) Frame->SetMaterial(0, FrameGlow);
	// The dust: Chladni nodal lines along the sheet (the figure a plate of sand makes on a bowed note), a band 2.4 m deep
	// across the floor; at rest it lies dark and scattered, with the tell it lights into the lines.
	FWRToolMesh Dm;
	const int32 Nx = 30, Ny = 12;
	const float Depth = 120.f;
	const int32 Na = 3 + (Ward % 3), Nb = 5 + (Ward % 2) * 2;
	const FColor Speck = FWRToolMesh::Pack(FLinearColor(FColor(216, 208, 255)), 1.f);
	for (int32 i = 0; i < Nx; ++i) for (int32 j = 0; j < Ny; ++j)
	{
		const float U = (i + 0.5f) / Nx, V = (j + 0.5f) / Ny;
		const float Fv = FMath::Cos(Na * PI * U) * FMath::Cos(Nb * PI * V) - FMath::Cos(Nb * PI * U) * FMath::Cos(Na * PI * V);
		if (FMath::Abs(Fv) > 0.16f) continue;
		const FVector C(-Depth + V * 2.f * Depth, -Hw + U * W.W, 2.f);
		Quad(Dm, C, FVector(0.f, W.W / Nx * 0.55f, 0.f), FVector(2.f * Depth / Ny * 0.3f, 0.f, 0.f), Speck);
	}
	Dust = Mesh(this, nullptr, TEXT("WardDust"));
	Dm.ApplyTo(Dust);
	DustGlow = GlowMID(this, FLinearColor(0.85f, 0.8f, 1.f), 0.f);
	if (DustGlow) Dust->SetMaterial(0, DustGlow);
}

void AWRWardField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.Wards.IsValidIndex(Ward)) return;
	if (HasAuthority()) StepServer();
	if (GetNetMode() == NM_DedicatedServer) return;
	// The tell, on every machine: an unattuned character within 3 m of the sheet (crystal 2.8): it rises over 0.9 s.
	const FWRCrystalData::FWard& W = D.Wards[Ward];
	float Want = 0.f;
	if (IsLive())
	{
		const float Range = Tn(TEXT("hazards.crystal.ward.tellRange"), 300.f);
		const FTransform& T = GetActorTransform();
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive()) continue;
			const FVector L = T.InverseTransformPosition(It->GetActorLocation());
			if (FMath::Abs(L.X) > Range || FMath::Abs(L.Y) > W.W * 0.5f + 150.f || L.Z < -200.f || L.Z > W.H + 150.f) continue;
			bool bHalf = false;
			if (IsAttuned(*It, bHalf)) continue;
			Want = 1.f;
			break;
		}
	}
	const float TellTime = FMath::Max(0.1f, Tn(TEXT("hazards.crystal.ward.tell"), 0.9f));
	Tell = FMath::FInterpConstantTo(Tell, Want, DeltaSeconds, 1.f / TellTime);
	const float Live = IsLive() ? 1.f : 0.f;
	if (FrameGlow) FrameGlow->SetScalarParameterValue(TEXT("Rest"), Live * (0.06f + Tell * 1.4f));
	if (DustGlow) DustGlow->SetScalarParameterValue(TEXT("Rest"), Live * Tell * 1.8f);
	// The hum: 49 Hz rising to 196 Hz over the tell (two octaves: the loop's pitch x1 to x4 from a low drone).
	if (Tell > 0.02f && !Hum) Hum = WRSound::StartLoop(this, TEXT("crystal.ward_hum"));
	if (Hum)
	{
		Hum->SetPitchMultiplier(FMath::Lerp(0.5f, 2.f, Tell));
		Hum->SetVolumeMultiplier(Tell);
		if (Tell <= 0.02f) { Hum->Stop(); Hum = nullptr; }
	}
}

void AWRWardField::StepServer()
{
	ServerClock -= GetWorld()->GetDeltaSeconds();
	if (ServerClock > 0.f) return;
	ServerClock = 0.05f;
	const FWRCrystalData::FWard& W = FWRCrystalData::Get().Wards[Ward];
	// The ward shift (crystal 2.10): pattern A normally, B in the Resonance.
	const bool bWant = ResonanceOn(this) ? W.bB : W.bA;
	if (bPattern != bWant) { bPattern = bWant; ForceNetUpdate(); }
	const FTransform& T = GetActorTransform();
	const float Now_ = Now(this);
	const bool bLive = IsLive();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive()) { Side.Remove(C); continue; }
		const FVector L = T.InverseTransformPosition(C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.5f));
		const float Cap = C->GetSimpleCollisionRadius();
		if (FMath::Abs(L.X) > 400.f || FMath::Abs(L.Y) > W.W * 0.5f + Cap || L.Z < -150.f || L.Z > W.H + 100.f) { Side.Remove(C); continue; }
		const float Prev = Side.FindRef(C);
		Side.Add(C, L.X);
		if (!bLive || Prev == 0.f || FMath::Sign(Prev) == FMath::Sign(L.X) || FMath::Abs(Prev - L.X) > 300.f) continue;
		// Crossed the plane.
		bool bHalf = false;
		if (IsAttuned(C, bHalf))
		{
			WRSound::PlayAt(this, TEXT("crystal.ward_pass"), C->GetActorLocation());
			continue;
		}
		const float Wait = Tn(TEXT("hazards.crystal.ward.interval"), 1.5f) + UWRInventoryComponent::GearStatFor(C, TEXT("wardRethrow"));
		if (Now_ - LastThrow.FindRef(C) < Wait) continue;
		LastThrow.Add(C, Now_);
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = Tn(TEXT("hazards.crystal.ward.damage"), 25.f) * (bHalf ? Tn(TEXT("hazards.crystal.ward.chalkDamageMult"), 0.5f) : 1.f);
		Hit.Element = WRTags::Element_Arcane;
		Hit.bUnblockable = true;
		Hit.StanceDamage = 0.f;
		Hit.PoiseDamage = 0.f;
		Hit.Impact = 1;
		Hit.Origin = C->GetActorLocation() - T.GetUnitAxis(EAxis::X) * FMath::Sign(Prev) * 100.f;
		UWRCombatLibrary::ApplyHit(C, Hit);
		WRFx::Broadcast(this, TEXT("crystal.ward_throw"), C->GetActorLocation(), T.GetRotation().Rotator(), 1.f, TEXT("crystal.ward_throw"));
		if (!bHalf)
		{
			// Thrown back the way they came, 6 m (a void creature twice as far: crystal 2.8).
			const FVector Back = T.GetUnitAxis(EAxis::X) * FMath::Sign(Prev);
			const float Mult = InSettingVoid(C) ? Tn(TEXT("hazards.crystal.ward.voidMult"), 2.f) : 1.f;
			C->LaunchCharacter(Back * Tn(TEXT("hazards.crystal.ward.throwSpeed"), 760.f) * Mult + FVector(0.f, 0.f, Tn(TEXT("hazards.crystal.ward.throwUp"), 420.f)), true, true);
			Side.Add(C, Prev);
		}
		UE_LOG(LogWildreach, Log, TEXT("CRYSTAL ward %s threw %s%s"), *W.Id.ToString(), *C->GetName(), bHalf ? TEXT(" (chalked: half, no throw)") : TEXT(""));
	}
}

void AWRWardField::Drop(float Seconds, AActor* By, bool bKeystone)
{
	if (!HasAuthority()) return;
	const bool bWasLive = IsLive();
	DownUntil = FMath::Max(DownUntil, Now(this) + Seconds);
	ForceNetUpdate();
	WRFx::Broadcast(this, TEXT("crystal.ward_drop"), GetActorLocation() + FVector(0.f, 0.f, 200.f), GetActorRotation(), 1.f, TEXT("crystal.ward_drop"));
	if (!bWasLive) return;
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL ward %s dropped %.0f s by %s"), *FWRCrystalData::Get().Wards[Ward].Id.ToString(), Seconds, *GetNameSafe(By));
	AWRCharacterBase* C = Cast<AWRCharacterBase>(By);
	AWRPlayerState* PS = C ? C->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS && bKeystone)
	{
		if (UWRWorldSystems* WS = GetWorld()->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("ward_dropped"), 1);
	}
}

// ================================================================================================ AWRWardKeystone

AWRWardKeystone::AWRWardKeystone()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	SetNetUpdateFrequency(1.f);
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetBoxExtent(FVector(8.f, 40.f, 40.f));
	Hitbox->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Hitbox;
}

void AWRWardKeystone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWardKeystone, Field);
}

void AWRWardKeystone::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	// A lead plate 70 cm square on a bronze bracket, a hexafoil inlaid in bronze (it glows while its field is up).
	FWRToolMesh M;
	const FLinearColor Lead(FColor(90, 92, 98)), Bronze(FColor(140, 106, 58));
	TArray<TArray<FVector>> Rings;
	for (const float X : { -6.f, 6.f }) Rings.Add({ FVector(X, -35.f, -35.f), FVector(X, 35.f, -35.f), FVector(X, 35.f, 35.f), FVector(X, -35.f, 35.f) });
	M.Loft(Rings, Lead * 0.55f, 0.6f, true);
	TArray<FVector> Bracket = { FVector(-6.f, 0.f, -35.f), FVector(-30.f, 0.f, -50.f), FVector(-60.f, 0.f, -50.f) };
	M.Tube(Bracket, 3.f, 6, Bronze * 0.6f, 0.45f);
	UProceduralMeshComponent* Body = Mesh(this, nullptr, TEXT("KeystoneBody"));
	M.ApplyTo(Body);
	FWRToolMesh H;
	Hexafoil(H, FVector(7.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector::UpVector, 9.f, 1.6f, Bronze, 1.f);
	Hexafoil(H, FVector(-7.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector::UpVector, 9.f, 1.6f, Bronze, 1.f);
	Plate = Mesh(this, nullptr, TEXT("KeystoneMark"));
	H.ApplyTo(Plate);
	Glow = GlowMID(this, FLinearColor(0.62f, 0.42f, 1.f), 0.4f);
	if (Glow) Plate->SetMaterial(0, Glow);
}

void AWRWardKeystone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Glow) return;
	const float Want = Field && Field->IsLive() ? 0.5f : 0.f;
	if (FMath::Abs(Want - Shown) > 0.01f) { Shown = Want; Glow->SetScalarParameterValue(TEXT("Rest"), Want); }
}

void AWRWardKeystone::Struck(AActor* By)
{
	if (!HasAuthority() || !Field) return;
	if (Field->IsLive()) Field->Drop(Tn(TEXT("hazards.crystal.ward.keystoneDrop"), 90.f), By, true);
	WRFx::Broadcast(this, TEXT("crystal.ring"), GetActorLocation(), FRotator::ZeroRotator, 0.6f, TEXT("crystal.ring"));
}

// ================================================================================================ AWRChandelier

AWRChandelier::AWRChandelier()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRChandelier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRChandelier, Phase);
	DOREPLIFETIME(AWRChandelier, PhaseAt);
	DOREPLIFETIME(AWRChandelier, TopZ);
	DOREPLIFETIME(AWRChandelier, FloorZ);
}

void AWRChandelier::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

float AWRChandelier::MassZ(float T) const
{
	// The mass's top: hung a third of the way down from the ring; falls under gravity; drawn back up over 20 s.
	const float Hung = TopZ - (TopZ - FloorZ) * Tn(TEXT("hazards.crystal.chandelier.hang"), 0.35f);
	const float Dt = FMath::Max(0.f, T - PhaseAt);
	if (Phase == 2)
	{
		const float G = Tn(TEXT("hazards.crystal.chandelier.gravity"), 980.f);
		return FMath::Max(FloorZ + 140.f, Hung - 0.5f * G * Dt * Dt);
	}
	if (Phase == 3) return FloorZ + 140.f;
	if (Phase == 4)
	{
		const float A = FMath::Clamp(Dt / Tn(TEXT("hazards.crystal.chandelier.rehangSeconds"), 20.f), 0.f, 1.f);
		return FMath::Lerp(FloorZ + 140.f, Hung, A * A * (3.f - 2.f * A));
	}
	return Hung;
}

void AWRChandelier::BuildLook()
{
	// The iron ring leaded into the ceiling and its bronze chain (crystal 2.8; Lechuguilla's Chandelier Ballroom): the
	// mass is a crown of crystal hanging point-down, the longest in the middle, lit lilac by the hall's noise.
	FWRToolMesh R;
	TArray<FVector> Ring;
	for (int32 k = 0; k < 16; ++k) { const float A = k * 2.f * PI / 16.f; Ring.Add(FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, -6.f)); }
	R.Tube(Ring, 5.f, 6, FLinearColor(0.03f, 0.028f, 0.027f), 0.7f, true);
	UProceduralMeshComponent* RingMesh = Mesh(this, nullptr, TEXT("ChandelierRing"));
	R.ApplyTo(RingMesh);
	Mass = NewObject<USceneComponent>(this, TEXT("ChandelierMass"));
	Mass->SetupAttachment(RootComponent);
	Mass->RegisterComponent();
	FWRToolMesh C;
	FRandomStream Rng(static_cast<int32>(GetTypeHash(GetName())));
	const FLinearColor Base(FColor(120, 90, 180)), Mid(FColor(200, 186, 240)), Tip(FColor(236, 230, 255));
	// The cap the chain hangs from, then the points: 9-15 hanging down, the outer ones flaring out.
	TArray<FVector> Cap;
	for (int32 k = 0; k < 12; ++k) { const float A = k * 2.f * PI / 12.f; Cap.Add(FVector(FMath::Cos(A) * 55.f, FMath::Sin(A) * 55.f, -10.f)); }
	C.Tube(Cap, 6.f, 6, FLinearColor(FColor(140, 106, 58)) * 0.6f, 0.45f, true);
	const int32 N = Rng.RandRange(9, 15);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float Out = i == 0 ? 0.f : Rng.FRandRange(18.f, 60.f);
		const FVector Root(FMath::Cos(A) * Out * 0.7f, FMath::Sin(A) * Out * 0.7f, -12.f);
		const FVector Dir = (FVector(FMath::Cos(A) * Out * 0.012f, FMath::Sin(A) * Out * 0.012f, -1.f)).GetSafeNormal();
		const float Len = i == 0 ? 160.f : Rng.FRandRange(50.f, 130.f);
		// One point (the same habit as the linings): six sides and a pyramid tip.
		FVector AY, AZ;
		Dir.FindBestAxisVectors(AY, AZ);
		const float Rr = Len * 0.12f;
		const FVector Sh = Root + Dir * Len * 0.8f, Tp = Root + Dir * Len;
		FVector R0[6], R1[6];
		for (int32 k = 0; k < 6; ++k) { const float B = k * PI / 3.f; const FVector O = (AY * FMath::Cos(B) + AZ * FMath::Sin(B)) * Rr; R0[k] = Root + O; R1[k] = Sh + O; }
		const FColor C0 = FWRToolMesh::Pack(Base, 0.3f), C1 = FWRToolMesh::Pack(Mid, 0.75f), C2 = FWRToolMesh::Pack(Tip, 1.f);
		const FVector2D Z = FVector2D::ZeroVector;
		for (int32 k = 0; k < 6; ++k) { const int32 n = (k + 1) % 6; C.Tri(R0[k], R1[k], R1[n], C0, Z, Z, Z); C.Tri(R0[k], R1[n], R0[n], C0, Z, Z, Z); C.Tri(R1[k], Tp, R1[n], C2, Z, Z, Z); }
		(void)C1;
	}
	UProceduralMeshComponent* MassMesh = Mesh(this, Mass, TEXT("ChandelierPoints"));
	C.ApplyTo(MassMesh);
	Glow = GlowMID(this, FLinearColor(0.75f, 0.65f, 1.f), 0.2f);
	if (Glow) MassMesh->SetMaterial(0, Glow);
	if (UWRCrystalWorld* W = UWRCrystalWorld::Get(this); W && Glow) W->RegisterGlow(Glow, GetActorLocation());
	// The chain: a bronze rod of links, 1 unit long down -Z, scaled to the gap.
	FWRToolMesh Ch;
	TArray<FVector> Links;
	for (int32 k = 0; k <= 20; ++k) Links.Add(FVector(FMath::Sin(k * 1.7f) * 0.8f, 0.f, -k / 20.f * 100.f));
	Ch.Tube(Links, 2.2f, 5, FLinearColor(FColor(140, 106, 58)) * 0.6f, 0.45f);
	Chain = Mesh(this, nullptr, TEXT("ChandelierChain"));
	Ch.ApplyTo(Chain);
	// What's left on the floor: a heap of broken points (shown once it has fallen).
	FWRToolMesh P;
	for (int32 i = 0; i < 26; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), Dd = Rng.FRandRange(0.f, 240.f);
		const FVector Root(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 0.f);
		const FVector Dir = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-0.1f, 0.5f)).GetSafeNormal();
		const float Len = Rng.FRandRange(20.f, 70.f);
		Quad(P, Root + Dir * Len * 0.5f + FVector(0, 0, 4.f), Dir * Len * 0.5f, FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal() * Len * 0.1f, FWRToolMesh::Pack(Mid, 0.8f));
	}
	Pile = Mesh(this, nullptr, TEXT("ChandelierPile"));
	P.ApplyTo(Pile);
	if (Glow) Pile->SetMaterial(0, Glow);
}

void AWRChandelier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = Now(this);
	if (HasAuthority())
	{
		if (Phase == 1 && T - PhaseAt >= Tn(TEXT("hazards.crystal.chandelier.tell"), 1.0f)) { Phase = 2; PhaseAt = T; ForceNetUpdate(); }
		else if (Phase == 2 && MassZ(T) <= FloorZ + 141.f) Land();
		else if (Phase == 4 && T - PhaseAt >= Tn(TEXT("hazards.crystal.chandelier.rehangSeconds"), 20.f)) { Phase = 0; PhaseAt = T; ForceNetUpdate(); }
	}
	if (GetNetMode() == NM_DedicatedServer || !Mass) return;
	// Every machine draws the same moment: shiver (the points tremble), fall, lie, rise.
	const float Z = MassZ(T);
	FRotator Shiver = FRotator::ZeroRotator;
	if (Phase == 1) Shiver = FRotator(FMath::Sin(T * 61.f) * 2.5f, FMath::Sin(T * 37.f) * 4.f, FMath::Sin(T * 53.f) * 2.5f);
	Mass->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Z));
	Mass->SetRelativeRotation(Shiver);
	Mass->SetVisibility(Phase != 3, true);
	if (Chain)
	{
		// The chain runs from the ring to the cap; once it has fallen only a broken stub hangs (and comes down with the
		// mass again as it is drawn up).
		const float Gap = Phase == 2 || Phase == 3 ? 60.f : FMath::Max(10.f, TopZ - Z);
		Chain->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, TopZ));
		Chain->SetWorldScale3D(FVector(1.f, 1.f, Gap / 100.f));
	}
	if (Pile) Pile->SetVisibility(Phase == 3);
	if (Pile && Phase == 3) Pile->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, FloorZ));
	if (Phase != ShownPhase)
	{
		if (Phase == 1) WRSound::PlayAt(this, TEXT("crystal.chandelier_tinkle"), FVector(GetActorLocation().X, GetActorLocation().Y, Z));
		if (Phase == 4) WRSound::PlayAt(this, TEXT("crystal.chandelier_rehang"), FVector(GetActorLocation().X, GetActorLocation().Y, TopZ));
		ShownPhase = Phase;
	}
}

void AWRChandelier::Heard(const FVector& At, float Radius)
{
	if (!HasAuthority() || Phase != 0) return;
	// A noise of 15 m or more that reaches the floor under it (crystal 2.8).
	if (Radius < Tn(TEXT("hazards.crystal.chandelier.noiseTrigger"), 1500.f)) return;
	if (FVector::Dist(At, FVector(GetActorLocation().X, GetActorLocation().Y, FloorZ)) > Radius) return;
	Shake();
}

void AWRChandelier::Shake()
{
	if (!HasAuthority() || Phase != 0) return;
	Phase = 1;
	PhaseAt = Now(this);
	ForceNetUpdate();
}

void AWRChandelier::Land()
{
	Phase = 3;
	PhaseAt = Now(this);
	ForceNetUpdate();
	const FVector At(GetActorLocation().X, GetActorLocation().Y, FloorZ);
	WRFx::Broadcast(this, TEXT("crystal.chandelier_crash"), At, FRotator::ZeroRotator, 1.f, TEXT("crystal.chandelier_crash"));
	WRCrystal::Emit(this, At, WRCrystal::NoiseRadius(TEXT("chime")) * 1.2f, this, TEXT("crash"));
	const float Reach = Tn(TEXT("hazards.crystal.chandelier.radius"), 300.f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || FVector::Dist2D(C->GetActorLocation(), At) > Reach + C->GetSimpleCollisionRadius() || FMath::Abs(C->GetActorLocation().Z - At.Z) > 400.f) continue;
		// The Geode Tyrant's counter (crystal 2.13.3): a chandelier on its back hurts it badly and stuns it.
		const AWRCreature* Cr = Cast<AWRCreature>(C);
		const bool bTyrant = Cr && Cr->CreatureId == TEXT("geode_tyrant");
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = bTyrant ? Tn(TEXT("hazards.crystal.chandelier.bossDamage"), 400.f) : Tn(TEXT("hazards.crystal.chandelier.damage"), 70.f);
		Hit.Element = WRTags::Element_Physical;
		Hit.Impact = uint8(Tn(TEXT("hazards.crystal.chandelier.impact"), 5.f));
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = At + FVector(0.f, 0.f, 300.f);
		UWRCombatLibrary::ApplyHit(C, Hit);
		if (bTyrant && C->Status) C->Status->ApplyStatus(WRTags::Status_Staggered, Tn(TEXT("hazards.crystal.chandelier.bossStun"), 6.f));
	}
	// It re-hangs after 2 game days (the chain draws it up over 20 s).
	const float GameHour = Tn(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f;
	GetWorldTimerManager().SetTimer(Rehang, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Phase = 4;
		PhaseAt = Now(this);
		ForceNetUpdate();
	}), FMath::Max(10.f, Tn(TEXT("hazards.crystal.chandelier.rehangHours"), 48.f) * GameHour), false);
}

// ================================================================================================ AWRThinFloor

AWRThinFloor::AWRThinFloor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetNetUpdateFrequency(1.f);
	Floor = CreateDefaultSubobject<UBoxComponent>(TEXT("Crust"));
	Floor->SetBoxExtent(FVector(400.f, 400.f, 10.f));
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Floor;
}

void AWRThinFloor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRThinFloor, bBroken);
	DOREPLIFETIME(AWRThinFloor, Radius);
}

void AWRThinFloor::BeginPlay()
{
	Super::BeginPlay();
	// The crust's top is the hall's floor: the box sits just under it, as wide as the hole under it.
	Floor->SetBoxExtent(FVector(Radius + 30.f, Radius + 30.f, 10.f));
	Floor->SetRelativeLocation(FVector::ZeroVector);
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	OnRep_Broken();
}

void AWRThinFloor::BuildLook()
{
	// A geode's lid of quartz sand cemented thin (crystal 2.8): a pale crust disc, hairline cracks across it lit violet
	// from the cup below; broken, a ring of jagged crust round the hole.
	FWRToolMesh C;
	const FColor Sand = FWRToolMesh::Pack(FLinearColor(FColor(198, 188, 206)) * 0.55f, 0.85f);
	const FVector2D Z = FVector2D::ZeroVector;
	const int32 Seg = 40;
	for (int32 k = 0; k < Seg; ++k)
	{
		const float A0 = k * 2.f * PI / Seg, A1 = (k + 1) * 2.f * PI / Seg;
		const float R0 = Radius * (1.f + 0.04f * FMath::Sin(A0 * 5.f)), R1 = Radius * (1.f + 0.04f * FMath::Sin(A1 * 5.f));
		C.Tri(FVector(0.f, 0.f, 11.f), FVector(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, 10.f), FVector(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, 10.f), Sand, Z, Z, Z);
	}
	Crust = Mesh(this, nullptr, TEXT("ThinCrust"));
	C.ApplyTo(Crust);
	FWRToolMesh K;
	FRandomStream Rng(static_cast<int32>(GetTypeHash(GetName())));
	for (int32 c = 0; c < 7; ++c)
	{
		FVector P(Rng.FRandRange(-0.3f, 0.3f) * Radius, Rng.FRandRange(-0.3f, 0.3f) * Radius, 11.5f);
		float A = Rng.FRandRange(0.f, 2.f * PI);
		for (int32 s = 0; s < 9; ++s)
		{
			A += Rng.FRandRange(-0.6f, 0.6f);
			const FVector Next = P + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rng.FRandRange(25.f, 55.f);
			if (Next.Size2D() > Radius * 0.95f) break;
			const FVector Mid = (P + Next) * 0.5f, Along = (Next - P) * 0.5f, Across = FVector::CrossProduct(Along.GetSafeNormal(), FVector::UpVector) * 1.2f;
			Quad(K, Mid, Along, Across, FWRToolMesh::Pack(FLinearColor(FColor(150, 100, 255)), 1.f));
			P = Next;
		}
	}
	UProceduralMeshComponent* Cracks = Mesh(this, nullptr, TEXT("ThinCracks"));
	K.ApplyTo(Cracks);
	if (UMaterialInstanceDynamic* G = GlowMID(this, FLinearColor(0.62f, 0.42f, 1.f), 0.6f)) Cracks->SetMaterial(0, G);
	Cracks->SetupAttachment(Crust);
	FWRToolMesh Rm;
	for (int32 k = 0; k < 28; ++k)
	{
		const float A = k * 2.f * PI / 28.f;
		const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector Base = Out * Radius;
		Quad(Rm, Base - Out * 25.f + FVector(0, 0, 8.f), Out * 28.f + FVector(0, 0, Rng.FRandRange(-6.f, 6.f)), FVector::CrossProduct(Out, FVector::UpVector) * Rng.FRandRange(14.f, 24.f), Sand);
	}
	Rim = Mesh(this, nullptr, TEXT("ThinRim"));
	Rm.ApplyTo(Rim);
}

void AWRThinFloor::OnRep_Broken()
{
	Floor->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (Crust) Crust->SetVisibility(!bBroken, true);
	if (Rim) Rim->SetVisibility(bBroken);
}

void AWRThinFloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBroken || GetNetMode() == NM_DedicatedServer) return;
	// Hollow underfoot (crystal 2.8): every step of yours within 6 m of it booms in the cup below.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* Me = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Me) return;
	BoomClock -= DeltaSeconds;
	const FVector Feet = Me->GetActorLocation() - FVector(0.f, 0.f, Me->GetSimpleCollisionHalfHeight());
	if (BoomClock > 0.f || FVector::Dist2D(Feet, GetActorLocation()) > Radius + Tn(TEXT("hazards.crystal.thin.hollowReach"), 600.f) || FMath::Abs(Feet.Z - GetActorLocation().Z) > 200.f) return;
	const float R = WRCrystal::StepRadius(Me);
	if (R <= 0.f) return;
	BoomClock = R >= WRCrystal::NoiseRadius(TEXT("run")) ? 0.32f : 0.48f;
	WRSound::PlayAt(this, TEXT("crystal.hollow"), Feet, FVector::Dist2D(Feet, GetActorLocation()) < Radius ? 1.f : 0.5f);
}

void AWRThinFloor::Heard(const FVector& At, float NoiseRadius, AActor* By, FName Kind)
{
	if (!HasAuthority() || bBroken) return;
	// A roll, a landing, a heavy blow, or a heavy creature's tread on the crust breaks it (walking doesn't).
	const AActor* Source = By ? By : nullptr;
	const FVector P = Kind == TEXT("heavy") && Source ? Source->GetActorLocation() - FVector(0.f, 0.f, 90.f) : At;
	if (FVector::Dist2D(P, GetActorLocation()) > Radius || FMath::Abs(P.Z - GetActorLocation().Z) > 220.f) return;
	const AWRCreature* Cr = Cast<AWRCreature>(By);
	const bool bHeavyBeast = Kind == TEXT("step") && Cr && Cr->GetSimpleCollisionRadius() >= Tn(TEXT("hazards.crystal.thin.heavyRadius"), 110.f);
	if (Kind == TEXT("roll") || Kind == TEXT("land") || Kind == TEXT("heavy") || bHeavyBeast) Break();
}

void AWRThinFloor::Break()
{
	bBroken = true;
	OnRep_Broken();
	ForceNetUpdate();
	WRFx::Broadcast(this, TEXT("crystal.thin_break"), GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("crystal.thin_break"));
	WRCrystal::Emit(this, GetActorLocation(), WRCrystal::NoiseRadius(TEXT("hitCrystal")), this, TEXT("crash"));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL thin floor %s broke"), *GetName());
	const float GameHour = Tn(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f;
	GetWorldTimerManager().SetTimer(Reform, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		// It reforms only with no one standing in the hole.
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Radius + 60.f && FMath::Abs(It->GetActorLocation().Z - GetActorLocation().Z) < 250.f) return;
		bBroken = false;
		OnRep_Broken();
		ForceNetUpdate();
		GetWorldTimerManager().ClearTimer(Reform);
	}), FMath::Max(10.f, Tn(TEXT("hazards.crystal.thin.reformHours"), 48.f) * GameHour), true);
}

// ================================================================================================ AWRSilentDoor

AWRSilentDoor::AWRSilentDoor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSilentDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSilentDoor, Petals);
	DOREPLIFETIME(AWRSilentDoor, OpenAt);
	DOREPLIFETIME(AWRSilentDoor, CloseAt);
}

FVector AWRSilentDoor::PadAt() const
{
	// The hexafoil lies 2.6 m before the door on the hall's side (the actor's -X), on the floor.
	return GetActorLocation() - GetActorForwardVector() * Tn(TEXT("hazards.crystal.vault.padOut"), 260.f);
}

float AWRSilentDoor::Slide(float T) const
{
	const float Roll = FMath::Max(0.2f, Tn(TEXT("hazards.crystal.vault.rollSeconds"), 3.f));
	float A = 0.f;
	if (OpenAt > 0.f) A = FMath::Clamp((T - OpenAt) / Roll, 0.f, 1.f);
	if (CloseAt > 0.f && T >= CloseAt) A = 1.f - FMath::Clamp((T - CloseAt) / Roll, 0.f, 1.f);
	return A * A * (3.f - 2.f * A);
}

void AWRSilentDoor::BeginPlay()
{
	Super::BeginPlay();
	Disc = NewObject<USceneComponent>(this, TEXT("VaultDisc"));
	Disc->SetupAttachment(RootComponent);
	Disc->RegisterComponent();
	// The door blocks the passage until it rolls aside (its box rides with it on every machine).
	Block = NewObject<UBoxComponent>(this, TEXT("VaultBlock"));
	Block->SetupAttachment(Disc);
	const float R = Tn(TEXT("hazards.crystal.vault.radius"), 300.f);
	Block->SetBoxExtent(FVector(25.f, R, R));
	Block->SetRelativeLocation(FVector(0.f, 0.f, R));
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	Block->RegisterComponent();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

void AWRSilentDoor::BuildLook()
{
	// A round door 6 m across, lead-plated over oak, a bronze hinge-ring round its rim, a rail in the floor it rolls on
	// into a slot in the wall; before it, a hexafoil 3 m across inlaid in the floor whose six petals hold a faint light.
	const float R = Tn(TEXT("hazards.crystal.vault.radius"), 300.f);
	const FLinearColor Lead(FColor(90, 92, 98)), Bronze(FColor(140, 106, 58));
	FWRToolMesh D;
	TArray<FVector> Prof = { FVector(0.f, R * 0.98f, R * 0.98f), FVector(0.5f, R, R), FVector(1.f, R * 0.98f, R * 0.98f) };
	D.Lathe(FVector(-20.f, 0.f, R), FVector(1.f, 0.f, 0.f), 40.f, Prof, 36, Lead * 0.55f, 0.62f);
	TArray<FVector> Rim;
	for (int32 k = 0; k < 40; ++k) { const float A = k * 2.f * PI / 40.f; Rim.Add(FVector(-22.f, FMath::Cos(A) * R, R + FMath::Sin(A) * R)); }
	D.Tube(Rim, 7.f, 6, Bronze * 0.6f, 0.45f, true);
	for (int32 k = 0; k < 12; ++k) { const float A = k * 2.f * PI / 12.f; TArray<FVector> Rivet = { FVector(-22.f, FMath::Cos(A) * R * 0.86f, R + FMath::Sin(A) * R * 0.86f), FVector(-27.f, FMath::Cos(A) * R * 0.86f, R + FMath::Sin(A) * R * 0.86f) }; D.Tube(Rivet, 5.f, 6, Bronze * 0.6f, 0.45f); }
	Hexafoil(D, FVector(-23.f, 0.f, R), FVector(0.f, 1.f, 0.f), FVector::UpVector, R * 0.3f, 3.f, Bronze * 0.6f, 0.45f);
	UProceduralMeshComponent* DiscMesh = Mesh(this, Disc, TEXT("VaultDiscMesh"));
	D.ApplyTo(DiscMesh);
	// The rail: two bronze bars in the floor from the doorway into the wall slot.
	FWRToolMesh Rl;
	for (const float X : { -30.f, 30.f }) { TArray<FVector> Bar = { FVector(X, -R * 1.1f, 2.f), FVector(X, R * 3.2f, 2.f) }; Rl.Tube(Bar, 4.f, 6, Bronze * 0.5f, 0.5f); }
	UProceduralMeshComponent* Rail = Mesh(this, nullptr, TEXT("VaultRail"));
	Rl.ApplyTo(Rail);
	// The hexafoil pad: six petals, each its own glow so they can go dark one by one.
	const FVector Pad = GetActorTransform().InverseTransformPosition(PadAt()) + FVector(0.f, 0.f, 2.f);
	FWRToolMesh Outline;
	Hexafoil(Outline, Pad, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), 50.f, 2.f, Bronze * 0.6f, 0.45f);
	UProceduralMeshComponent* PadMesh = Mesh(this, nullptr, TEXT("VaultPad"));
	Outline.ApplyTo(PadMesh);
	for (int32 p = 0; p < 6; ++p)
	{
		FWRToolMesh Pm;
		const float A = p * PI / 3.f;
		const FVector C = Pad + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 50.f;
		// A petal: the lens where a circle overlaps its neighbour (drawn as a fan of the circle's arc toward the centre).
		const FColor Col = FWRToolMesh::Pack(FLinearColor(FColor(216, 208, 255)), 1.f);
		for (int32 k = 0; k < 10; ++k)
		{
			const float B0 = A + PI * (0.7f + k * 0.06f), B1 = A + PI * (0.7f + (k + 1) * 0.06f);
			const FVector2D Z = FVector2D::ZeroVector;
			Pm.Tri(C + FVector(0, 0, 1.f), C + FVector(FMath::Cos(B0), FMath::Sin(B0), 0.f) * 46.f + FVector(0, 0, 1.f), C + FVector(FMath::Cos(B1), FMath::Sin(B1), 0.f) * 46.f + FVector(0, 0, 1.f), Col, Z, Z, Z);
			Pm.Tri(C + FVector(0, 0, 1.f), C + FVector(FMath::Cos(B1), FMath::Sin(B1), 0.f) * 46.f + FVector(0, 0, 1.f), C + FVector(FMath::Cos(B0), FMath::Sin(B0), 0.f) * 46.f + FVector(0, 0, 1.f), Col, Z, Z, Z);
		}
		UProceduralMeshComponent* PetalMesh = Mesh(this, nullptr, *FString::Printf(TEXT("VaultPetal%d"), p));
		Pm.ApplyTo(PetalMesh);
		UMaterialInstanceDynamic* G = GlowMID(this, FLinearColor(0.85f, 0.8f, 1.f), 0.9f);
		if (G) PetalMesh->SetMaterial(0, G);
		PetalGlow.Add(G);
	}
}

void AWRSilentDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = Now(this);
	if (HasAuthority())
	{
		const float Roll = Tn(TEXT("hazards.crystal.vault.rollSeconds"), 3.f);
		if (OpenAt > 0.f && CloseAt <= 0.f && T - OpenAt >= Roll + Tn(TEXT("hazards.crystal.vault.openSeconds"), 60.f))
		{
			// It rolls back once the doorway is clear (never onto anyone).
			bool bClear = true;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It) if (FVector::Dist(It->GetActorLocation(), GetActorLocation() + FVector(0, 0, 200.f)) < 380.f) bClear = false;
			if (bClear) { CloseAt = T; ForceNetUpdate(); }
		}
		if (CloseAt > 0.f && T - CloseAt >= Roll) { OpenAt = 0.f; CloseAt = 0.f; Petals = 6; QuietSince = T; ForceNetUpdate(); }
		if (OpenAt <= 0.f)
		{
			// Someone stands on the hexafoil and no sound within 20 m for 10 s: the petals go dark one each 1.7 s.
			bool bOn = false;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !It->GetPlayerState()) continue;
				const FVector F = It->GetActorLocation() - FVector(0.f, 0.f, It->GetSimpleCollisionHalfHeight());
				if (FVector::Dist2D(F, PadAt()) < Tn(TEXT("hazards.crystal.vault.padRadius"), 150.f) && FMath::Abs(F.Z - PadAt().Z) < 150.f) bOn = true;
			}
			const bool bLocked = ResonanceOn(this);
			if (!bOn || bLocked)
			{
				if (Petals != 6) { Petals = 6; ForceNetUpdate(); }
				QuietSince = T;
				NextPetalAt = 0.f;
			}
			else if (T - QuietSince >= Tn(TEXT("hazards.crystal.vault.quietSeconds"), 10.f))
			{
				// The first petal goes dark at once at 10 s, the rest one each 1.7 s.
				if (NextPetalAt <= 0.f) NextPetalAt = T;
				if (T >= NextPetalAt && Petals > 0)
				{
					--Petals;
					NextPetalAt = T + Tn(TEXT("hazards.crystal.vault.petalSeconds"), 1.7f);
					ForceNetUpdate();
					if (Petals == 0)
					{
						OpenAt = T;
						CloseAt = 0.f;
						WRSound::PlayAt(this, TEXT("crystal.vault_door"), GetActorLocation());
						UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Silent Vault's door rolls aside"));
					}
				}
			}
		}
	}
	if (Disc)
	{
		const float R = Tn(TEXT("hazards.crystal.vault.radius"), 300.f);
		Disc->SetRelativeLocation(FVector(0.f, Slide(T) * (2.f * R + 40.f), 0.f));
		Disc->SetRelativeRotation(FRotator(0.f, 0.f, -Slide(T) * FMath::RadiansToDegrees((2.f * R + 40.f) / R)));
	}
	if (Petals != ShownPetals && GetNetMode() != NM_DedicatedServer)
	{
		for (int32 p = 0; p < PetalGlow.Num(); ++p) if (PetalGlow[p]) PetalGlow[p]->SetScalarParameterValue(TEXT("Rest"), p < Petals ? 0.9f : 0.f);
		if (ShownPetals != 255 && Petals < ShownPetals) WRSound::PlayAt(this, TEXT("crystal.vault_petal"), PadAt(), 0.6f);
		ShownPetals = Petals;
	}
}

void AWRSilentDoor::Heard(const FVector& At, float Radius)
{
	if (!HasAuthority() || OpenAt > 0.f) return;
	if (FVector::Dist(At, PadAt()) > FMath::Max(Radius, Tn(TEXT("hazards.crystal.vault.hearRadius"), 2000.f))) return;
	// Any sound relights all six (and the quiet starts over).
	QuietSince = Now(this);
	NextPetalAt = 0.f;
	if (Petals != 6) { Petals = 6; ForceNetUpdate(); WRSound::PlayAt(this, TEXT("crystal.vault_relight"), PadAt()); }
}

// ================================================================================================ items, the Vault's rule

FString WRCrystal::UseItem(AWRPlayerState* PS, FName Item, int32 Slot)
{
	if (Item != TEXT("attuning_chalk") && Item != TEXT("ward_fork")) return FString();
	AWRCharacterBase* Me = PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Me || !Inv || !Me->IsAlive()) return TEXT("Not now");
	if (Item == TEXT("attuning_chalk"))
	{
		// Drawn on the forearms (a breath's work): 5 minutes, half a ward's harm and no throw (crystal 2.7).
		if (Me->Status) Me->Status->ApplyStatus(WRTags::Status_Attuned, Tn(TEXT("hazards.crystal.ward.chalkSeconds"), 300.f));
		Inv->RemoveItem(Item, 1);
		WRFx::BroadcastOn(Me, TEXT("crystal.chalk"), NAME_None, 1.f, TEXT("crystal.chalk"));
		return TEXT("Chalk on your forearms: the wards will hardly know you for five minutes");
	}
	// A ward fork: the nearest live field within 10 m falls quiet for a minute.
	AWRWardField* Best = nullptr;
	float BestD = Tn(TEXT("hazards.crystal.ward.forkReach"), 1000.f);
	for (TActorIterator<AWRWardField> It(Me->GetWorld()); It; ++It)
	{
		if (!It->IsLive()) continue;
		const float Dd = FVector::Dist(It->GetActorLocation() + FVector(0, 0, 200.f), Me->GetActorLocation());
		if (Dd < BestD) { BestD = Dd; Best = *It; }
	}
	if (!Best) return TEXT("No live ward within 10 m to quiet");
	Inv->RemoveItem(Item, 1);
	WRSound::PlayAt(Me, TEXT("crystal.fork"), Me->GetActorLocation(), 2.f);
	Best->Drop(Tn(TEXT("hazards.crystal.ward.forkSeconds"), 60.f), Me, false);
	return TEXT("The fork sings, and the ward goes still");
}

void WRCrystal::LanternLit(AWRCharacterBase* Who, FName LightItem)
{
	if (!Who || !Who->HasAuthority()) return;
	const float Flare = WRTune(*FString::Printf(TEXT("lights.%s.flare"), *LightItem.ToString()), 0.f);
	if (Flare <= 0.f) return;
	// Struck awake: it flares 20 m for 5 s, and rings (a 10 m noise in the caves).
	WRFx::BroadcastOn(Who, TEXT("crystal.lumen_flare"), NAME_None, 1.f, TEXT("crystal.lumen_strike"), WRTune(*FString::Printf(TEXT("lights.%s.flareSeconds"), *LightItem.ToString()), 5.f));
	Emit(Who, Who->GetActorLocation(), NoiseRadius(TEXT("cast")), Who, TEXT("flare"));
}

bool WRCrystal::SpellFails(const AActor* Caster)
{
	return Caster && Live() && InVault(Caster->GetActorLocation());
}

// ================================================================================================ the server's hazards

void UWRCrystalWorld::SpawnHazards()
{
	UWorld* World = GetWorld();
	const FWRCrystalData& D = FWRCrystalData::Get();
	// Ward fields and their keystones.
	for (int32 i = 0; i < D.Wards.Num(); ++i)
	{
		const FWRCrystalData::FWard& W = D.Wards[i];
		const FTransform Tr(FRotator(0.f, W.Yaw, 0.f), W.At);
		AWRWardField* F = World->SpawnActorDeferred<AWRWardField>(AWRWardField::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!F) continue;
		F->Ward = i;
		F->bPattern = W.bA;
		F->FinishSpawning(Tr);
		// The keystone faces its field (it's set in sight of it).
		const FVector ToField = (W.At + FVector(0, 0, W.H * 0.5f) - W.Key).GetSafeNormal();
		const FTransform Kt(FRotator(0.f, ToField.Rotation().Yaw, 0.f), W.Key);
		AWRWardKeystone* K = World->SpawnActorDeferred<AWRWardKeystone>(AWRWardKeystone::StaticClass(), Kt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (K) { K->Field = F; K->FinishSpawning(Kt); }
	}
	// Chandeliers (the fallen one is scenery).
	for (const FWRCrystalData::FChandelier& C : D.Chandeliers)
	{
		if (C.bFallen) continue;
		const FTransform Tr(FRotator::ZeroRotator, C.Top);
		AWRChandelier* A = World->SpawnActorDeferred<AWRChandelier>(AWRChandelier::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) continue;
		A->TopZ = C.Top.Z;
		A->FloorZ = C.Floor;
		A->FinishSpawning(Tr);
		OnNoise.AddWeakLambda(A, [A](const FVector& At, float R, AActor*, FName) { A->Heard(At, R); });
	}
	// Thin floors over their cups.
	for (const FWRCrystalData::FThin& T : D.Thin)
	{
		const FTransform Tr(FRotator::ZeroRotator, T.At - FVector(0.f, 0.f, 10.f));
		AWRThinFloor* A = World->SpawnActorDeferred<AWRThinFloor>(AWRThinFloor::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) continue;
		A->Radius = T.R;
		A->FinishSpawning(Tr);
		OnNoise.AddWeakLambda(A, [A](const FVector& At, float R, AActor* By, FName Kind) { A->Heard(At, R, By, Kind); });
	}
	// The Silent Vault's door: where the passage W6 -> V1 leaves the Hall of the Door.
	if (const FWRCrystalData::FTube* Tb = D.FindTube(TEXT("W6-V1")))
	{
		const FWRCrystalData::FHall* H = D.FindHall(TEXT("W6"));
		const int32 HallIx = H ? int32(H - D.Halls.GetData()) : INDEX_NONE;
		int32 k = 0;
		while (HallIx != INDEX_NONE && k + 1 < Tb->Pts.Num() && D.ShapeAir(HallIx, Tb->Pts[k] + FVector(0, 0, 150.f)) < 0.f) ++k;
		k = FMath::Clamp(k + 1, 1, Tb->Pts.Num() - 1);
		const FVector Dir = (Tb->Pts[k] - Tb->Pts[k - 1]).GetSafeNormal2D();
		const FTransform Tr(Dir.Rotation(), Tb->Pts[k]);
		if (AWRSilentDoor* V = World->SpawnActor<AWRSilentDoor>(AWRSilentDoor::StaticClass(), Tr))
		{
			OnNoise.AddWeakLambda(V, [V](const FVector& At, float R, AActor*, FName) { V->Heard(At, R); });
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL hazards: %d wards, %d chandeliers, %d thin floors, the Vault door"), D.Wards.Num(), D.Chandeliers.Num(), D.Thin.Num());
}

void UWRCrystalWorld::StepPools(float Dt)
{
	// Scalding pools (crystal 2.8): wading burns (15 burn buildup a second and 4 damage a second), full immersion 12 a
	// second; Burning blood passes, fire resistance halves. Checked twice a second.
	PoolClock -= Dt;
	if (PoolClock > 0.f) return;
	const float Step = 0.5f;
	PoolClock = Step;
	const FWRCrystalData& D = FWRCrystalData::Get();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || CrystalBorn(C)) continue;
		const FVector P = C->GetActorLocation();
		const float Half = C->GetSimpleCollisionHalfHeight();
		for (const FWRCrystalData::FPool& Pl : D.Pools)
		{
			if (Pl.Kind != TEXT("scald")) continue;
			const FVector2D L(P.X - Pl.At.X, P.Y - Pl.At.Y);
			if (FMath::Square(L.X / Pl.Rx) + FMath::Square(L.Y / Pl.Ry) > 1.f) continue;
			const float Feet = P.Z - Half;
			if (Feet > Pl.At.Z || Feet < Pl.At.Z - Pl.Depth - 200.f) continue;
			if (C->HasTag(WRTags::Passive_BurningBlood)) break;
			const bool bUnder = P.Z + Half < Pl.At.Z;
			const float Resist = UWRInventoryComponent::GearStatFor(C, TEXT("fireResist")) > 0.f || C->HasTag(WRTags::Resist_Lava) ? 0.5f : 1.f;
			FWRHit Hit;
			Hit.Instigator = nullptr;
			Hit.Damage = (bUnder ? Tn(TEXT("hazards.crystal.scald.immersedDps"), 12.f) : Tn(TEXT("hazards.crystal.scald.dps"), 4.f)) * Step * Resist;
			Hit.BurnBuildup = bUnder ? 0.f : Tn(TEXT("hazards.crystal.scald.burnPerSecond"), 15.f) * Step * Resist;
			Hit.Element = WRTags::Element_Fire;
			Hit.bDamageOverTime = true;
			Hit.bUnblockable = true;
			Hit.StanceDamage = 0.f;
			UWRCombatLibrary::ApplyHit(C, Hit);
			break;
		}
	}
}

// ================================================================================================ the Resonance

void UWRCrystalWorld::StepResonance(float Dt)
{
	// The Resonance (crystal 2.10; world.json events.resonance, scheduled by the world systems): a 5 minute warning (the
	// 40 Hz swell from every cave mouth, the Crown blazing, chime stones ringing on their own), then a game day of peals
	// every 4 minutes rolling out of the Prism Hall at 300 m/s: every crystal flares, chandeliers in its path fall, the
	// Echobats go deaf, the nests crack. The first peal regrows every resonant node and splits the six singing geodes.
	// Wards switch to pattern B (AWRWardField), the Vault won't open (AWRSilentDoor), resonant crystal pays x3.
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("resonance")) : nullptr;
	const float T = Now(World);
	const bool bWarn = E && E->Stage == 0 && T < E->StartAt;
	const bool bOn = E && E->Stage == 1 && T < E->EndAt;
	const float Every = FMath::Max(10.f, Tn(TEXT("crystalEvents.resonance.pealEvery"), 240.f));
	const float Speed = FMath::Max(100.f, Tn(TEXT("crystalEvents.resonance.pealSpeed"), 30000.f));
	const FWRCrystalData& D = FWRCrystalData::Get();
	const FWRCrystalData::FHall* Prism = D.FindHall(TEXT("W5"));
	const FVector Source = Prism ? Prism->Centre() : FVector::ZeroVector;
	if (World->GetNetMode() != NM_Client)
	{
		ResonanceYield = bOn ? Tn(TEXT("crystalEvents.resonance.payout"), 3.f) : 1.f;
		if (bOn)
		{
			const int32 K = FMath::FloorToInt((T - E->StartAt) / Every);
			while (PealsDone <= K) Peal(PealsDone++, Source, Speed);
		}
		else if (PealsDone > 0)
		{
			// The last peal fades; the singing geodes close again (whatever wasn't taken goes back into the rock).
			PealsDone = 0;
			for (const TWeakObjectPtr<AWRCrystalSpot>& G : SingingGeodes) if (G.IsValid()) G->Destroy();
			SingingGeodes.Reset();
			UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Resonance is over"));
		}
	}
	if (World->GetNetMode() == NM_DedicatedServer) return;
	FVector View = FVector::ZeroVector;
	if (const APlayerController* PC = World->GetFirstPlayerController()) { FRotator R; PC->GetPlayerViewPoint(View, R); }
	const bool bHere = WRCrystal::Contains(View);
	// The glow: half-lit all through the event, rising through the warning; each peal a 6 s flare as it passes you.
	float Glow = bOn ? Tn(TEXT("crystalEvents.resonance.glow"), 0.5f) : bWarn ? Tn(TEXT("crystalEvents.resonance.glow"), 0.5f) * FMath::Clamp(1.f - (E->StartAt - T) / 300.f, 0.f, 1.f) : 0.f;
	if (bOn)
	{
		const float Pass = FVector::Dist(View, Source) / Speed;
		const float Since = FMath::Fmod(FMath::Max(0.f, T - E->StartAt - Pass), Every);
		const float Len = Tn(TEXT("crystalEvents.resonance.pealSeconds"), 6.f);
		if (T - E->StartAt >= Pass && Since < Len)
		{
			Glow += Tn(TEXT("crystalEvents.resonance.pealGlow"), 4.f) * (1.f - Since / Len);
			const int32 K = FMath::FloorToInt((T - E->StartAt - Pass) / Every);
			if (K != PealHeard && bHere)
			{
				PealHeard = K;
				WRSound::PlayAt(World, TEXT("crystal.peal"), View);
				WRFx::Shake(World, Tn(TEXT("crystalEvents.resonance.shake"), 0.15f));
			}
		}
	}
	ResonanceGlow = Glow;
	// The swell under everything while it warns (felt more than heard).
	AActor* Host = CavesActor.Get();
	if (bWarn && bHere && !Swell.IsValid() && Host) Swell = WRSound::StartLoop(Host, TEXT("crystal.resonance_swell"));
	if (UAudioComponent* Sw = Swell.Get())
	{
		Sw->SetVolumeMultiplier(bWarn && bHere ? FMath::Clamp(1.f - (E->StartAt - T) / 300.f, 0.1f, 1.f) : 0.f);
		if (!(bWarn && bHere)) { Sw->Stop(); Swell = nullptr; }
	}
	// Chime stones on the plateau ring by themselves through the warning (a few at a time, near you).
	if (bWarn && bHere)
	{
		ChimeClock -= Dt;
		if (ChimeClock <= 0.f)
		{
			ChimeClock = FMath::FRandRange(1.2f, 3.5f);
			for (int32 Tries = 0; Tries < 8; ++Tries)
			{
				const FWRCrystalData::FChime& C = D.Chimes[FMath::RandRange(0, FMath::Max(0, D.Chimes.Num() - 1))];
				if (FVector::Dist(C.At, View) < 9000.f) { WRSound::PlayAt(World, TEXT("crystal.ring"), C.At + FVector(0, 0, 150.f), 0.7f); break; }
			}
		}
	}
}

void UWRCrystalWorld::Peal(int32 Index, const FVector& Source, float Speed)
{
	UWorld* World = GetWorld();
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Resonance's peal %d"), Index + 1);
	// Chandeliers fall as the wave reaches them.
	for (TActorIterator<AWRChandelier> It(World); It; ++It)
	{
		TWeakObjectPtr<AWRChandelier> Weak(*It);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) Weak->Shake(); }), FMath::Max(0.05f, FVector::Dist(It->GetActorLocation(), Source) / Speed), false);
	}
	OnPeal.Broadcast(Index, Source, Speed);
	if (Index != 0) return;
	// The first peal: every resonant node regrows at once, and the singing geodes split along their seams.
	int32 Regrown = 0;
	for (const TWeakObjectPtr<AWRResourceNode>& N : Nodes)
	{
		if (N.IsValid() && N->ItemId == TEXT("resonant_crystal") && N->IsDepleted()) { N->RegrowNow(); ++Regrown; }
	}
	const TArray<FWRCrystalData::FNode>* Singing = FWRCrystalData::Get().Resources.Find(TEXT("singing"));
	if (Singing)
	{
		static const TCHAR* Runes[] = { TEXT("rune_prism"), TEXT("rune_warding"), TEXT("rune_doubling") };
		for (const FWRCrystalData::FNode& G : *Singing)
		{
			const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), G.At);
			AWRCrystalSpot* Sp = World->SpawnActorDeferred<AWRCrystalSpot>(AWRCrystalSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Sp) continue;
			Sp->Kind = TEXT("geode");
			Sp->SpotId = TEXT("singing_geode");
			Sp->Title = TEXT("A singing geode, split open");
			Sp->Verb = TEXT("Take what's inside");
			Sp->Gives.Add(TEXT("resonant_crystal"), int32(Tn(TEXT("crystalEvents.resonance.geodeYield"), 12.f)));
			Sp->Gives.Add(TEXT("gem_amethyst"), 1);
			for (const TCHAR* R : Runes) if (FMath::FRand() < Tn(TEXT("crystalEvents.resonance.runeChance"), 0.1f)) Sp->Gives.Add(R, 1);
			Sp->FinishSpawning(Tr);
			SingingGeodes.Add(Sp);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the first peal: %d resonant nodes regrown, %d singing geodes split"), Regrown, SingingGeodes.Num());
}
