// The Crystal caves' live things (see WRCrystalThings.h; cooks/build-biome-crystal.md 2.6, 2.7, 2.13).
//
// Sound makes light (crystal 2.1): one noise model. The server hears every noise (WRCrystal::Emit: blows, casts, rolls,
// landings, chimes, strikes on crystal, arrows and stones landing; footsteps sampled four times a second) and hands it
// to its listeners (UWRCrystalWorld::OnNoise); every noise but a footstep goes to every machine as one small unreliable
// message, which lights the crystals round it: each hall's lining has four rings (a point, a radius, a strength that
// falls to nothing over 6 s), the strongest of which also drive the hall's lights. Footsteps light the crystals on
// each machine from the characters it sees (the same rule, no traffic).

#include "WRCrystalThings.h"
#include "WRCrystalWorld.h"
#include "WRCrystalCaves.h"
#include "WRCrystalTypes.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"

#include "WRWorldSystems.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace WRCrystalThingsImpl
{
	float CtTn(const TCHAR* Key, float Def) { return WRTune(Key, Def); }
	float CtTn(const FString& Key, float Def) { return WRTune(*Key, Def); }

	AWRPlayerController* CzControllerOf(const AActor* Pawn)
	{
		const APawn* P = Cast<APawn>(Pawn);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}
	void CtTell(const AActor* Pawn, const FString& Text)
	{
		if (AWRPlayerController* PC = CzControllerOf(Pawn)) PC->ClientNotify(Text);
	}
	double CzGameHours(const UObject* Ctx)
	{
		const AWRGameState* GS = AWRGameState::Get(Ctx);
		return GS ? GS->GetTotalHours() : 0.0;
	}
	FString CzItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
	/** How brightly a noise of this radius lights the crystals round it (crystal 2.1: louder is brighter and wider). */
	float CzStrengthFor(float Radius)
	{
		return CtTn(TEXT("crystalWorld.light.strength"), 1.6f) * FMath::Sqrt(FMath::Clamp(Radius / 2000.f, 0.05f, 2.f));
	}
	/** Raining on the Crystal caves' surface now (the drip song, crystal 2.9). */
	bool CzRaining(const UObject* Ctx)
	{
		const AWRGameState* GS = AWRGameState::Get(Ctx);
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (!GS || !W) return false;
		const float Now = W->GetTimeSeconds();
		for (const FWRWeatherNow& N : GS->Weather) if (N.Biome == WRCrystal::Biome() && N.Kind == TEXT("rain") && N.IsOn(Now)) return true;
		return false;
	}

	/** One quartz point (the real habit): a six-sided prism from Root along Dir, its six-faced pyramid tip. Vertex alpha
	 *  is how much of it glows in M_CrystalGlow (0.3 at the root to 1 at the tip). */
	void CzPoint(FWRToolMesh& M, const FVector& Root, const FVector& Dir, float Len, float Thick, const FLinearColor& Base, const FLinearColor& Mid, const FLinearColor& Tip, float Turn)
	{
		FVector AY, AZ;
		Dir.FindBestAxisVectors(AY, AZ);
		const float R = Thick * 0.5f;
		const FVector Sh = Root + Dir * (Len * 0.78f), Tp = Root + Dir * Len;
		const FColor C0 = FWRToolMesh::Pack(Base, 0.3f), C1 = FWRToolMesh::Pack(Mid, 0.7f), C2 = FWRToolMesh::Pack(Tip, 1.f);
		FVector R0[6], R1[6];
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = Turn + k * PI / 3.f;
			const FVector O = (AY * FMath::Cos(A) + AZ * FMath::Sin(A)) * R;
			R0[k] = Root + O * 0.92f;
			R1[k] = Sh + O;
		}
		const FVector2D Z = FVector2D::ZeroVector;
		for (int32 k = 0; k < 6; ++k)
		{
			const int32 n = (k + 1) % 6;
			M.Tri(R0[k], R1[n], R1[k], C0, Z, Z, Z);
			M.Tri(R0[k], R0[n], R1[n], C0, Z, Z, Z);
			M.Tri(R1[k], R1[n], Tp, C1, Z, Z, Z);
		}
		(void)C2;
	}
}

using namespace WRCrystalThingsImpl;

// ================================================================================================ noise

void WRCrystal::Emit(const UObject* WorldContext, const FVector& At, float Radius, AActor* Instigator, FName Kind)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || Radius <= 0.f || !Live()) return;
	UWRCrystalWorld* W = UWRCrystalWorld::Get(World);
	if (!W || !W->bActive || !Contains(At)) return;
	// The Rune of Silence (and anything else that quiets you): worn noiseMult.
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Instigator); C && C->GetPlayerState())
	{
		Radius *= FMath::Max(0.f, 1.f + UWRInventoryComponent::GearStatFor(C, TEXT("noiseMult")));
		// The Warden's tuning fork (talisman): your steps, rolls and landings make half the noise.
		if (Kind == TEXT("step") || Kind == TEXT("roll") || Kind == TEXT("land")) Radius *= FMath::Max(0.f, 1.f - UWRInventoryComponent::GearStatFor(C, TEXT("quietSteps")));
		if (Radius <= 1.f) return;
	}
	W->OnNoise.Broadcast(At, Radius, Instigator, Kind);
	if (Kind == TEXT("step")) return;
	if (AWRCrystalState* S = AWRCrystalState::Get(World)) S->MulticastNoise(At, Radius, CzStrengthFor(Radius));
}

float WRCrystal::NoiseRadius(FName Kind)
{
	return CtTn(FString::Printf(TEXT("crystalWorld.noise.%s"), *Kind.ToString()), 0.f);
}

float WRCrystal::StepRadius(const AActor* Who)
{
	const ACharacter* C = Cast<ACharacter>(Who);
	const UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	if (!M || M->IsFalling() || M->IsSwimming() || C->bIsCrouched) return 0.f;
	const float Speed = C->GetVelocity().Size2D();
	if (Speed < CtTn(TEXT("crystalWorld.noise.stillBelow"), 40.f)) return 0.f;
	float R = Speed < CtTn(TEXT("crystalWorld.noise.walkBelow"), 360.f) ? NoiseRadius(TEXT("walk"))
		: Speed < CtTn(TEXT("crystalWorld.noise.runBelow"), 620.f) ? NoiseRadius(TEXT("run")) : NoiseRadius(TEXT("sprint"));
	// A big creature treads louder (a Druseback, a Lumenhorn): its capsule's radius over a person's.
	if (const AWRCreature* Cr = Cast<AWRCreature>(Who)) R *= FMath::Clamp(Cr->GetSimpleCollisionRadius() / 45.f, 0.5f, 3.f);
	return R;
}

// ================================================================================================ AWRCrystalState

AWRCrystalState::AWRCrystalState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCrystalState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCrystalState, HallLitUntil);
}

AWRCrystalState* AWRCrystalState::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRCrystalState> It(World); It; ++It) return *It;
	return nullptr;
}

void AWRCrystalState::MulticastNoise_Implementation(FVector_NetQuantize At, float Radius, float Strength)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (UWRCrystalWorld* W = UWRCrystalWorld::Get(this)) W->Light(At, Radius, Strength, nullptr);
}

bool AWRCrystalState::IsHallLit(int32 Hall) const
{
	return HallLitUntil.IsValidIndex(Hall) && HallLitUntil[Hall] > CzGameHours(this);
}

// ================================================================================================ AWRCrystalSpot

AWRCrystalSpot::AWRCrystalSpot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(1.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCrystalSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCrystalSpot, Kind);
	DOREPLIFETIME(AWRCrystalSpot, SpotId);
	DOREPLIFETIME(AWRCrystalSpot, Title);
	DOREPLIFETIME(AWRCrystalSpot, Verb);
	DOREPLIFETIME(AWRCrystalSpot, Hall);
}

void AWRCrystalSpot::BeginPlay()
{
	Super::BeginPlay();
	if (UWRCrystalWorld* W = UWRCrystalWorld::Get(this)) W->AddSpot(this);
	// A keepsake or a find shows itself by a faint lilac mote over where it lies (the scenery draws the thing itself).
	if (GetNetMode() != NM_DedicatedServer && (Kind == TEXT("keepsake") || Kind == TEXT("find") || Kind == TEXT("geode")))
	{
		Glint = WRFx::Attach(GetRootComponent(), TEXT("crystal.keepsake"), NAME_None, 1.f);
	}
	if (GetNetMode() != NM_DedicatedServer && Kind == TEXT("geode"))
	{
		// A singing geode split along its seam (crystal 2.10): two basalt half-shells 1.6 m across fallen apart, agate
		// bands at the break, the violet druse inside each lit by the peals.
		FWRToolMesh Sh;
		const float R = 80.f;
		for (const float Side : { -1.f, 1.f })
		{
			TArray<TArray<FVector>> Rings;
			const FRotator Open(0.f, 0.f, Side * 28.f);
			for (int32 b = 0; b <= 6; ++b)
			{
				const float Ph = b / 6.f * PI * 0.5f;
				TArray<FVector> Ring;
				for (int32 a = 0; a <= 12; ++a)
				{
					const float Th = a / 12.f * PI;
					const FVector P(FMath::Cos(Th) * FMath::Cos(Ph) * R, Side * FMath::Sin(Ph) * R * 0.95f + Side * 12.f, FMath::Sin(Th) * FMath::Cos(Ph) * R * 0.85f);
					Ring.Add(Open.RotateVector(P) + FVector(0.f, 0.f, 6.f));
				}
				Rings.Add(Ring);
			}
			const FVector2D Z = FVector2D::ZeroVector;
			for (int32 b = 0; b < 6; ++b) for (int32 a = 0; a < 12; ++a)
			{
				const FVector P0 = Rings[b][a], P1 = Rings[b][a + 1], P2 = Rings[b + 1][a + 1], P3 = Rings[b + 1][a];
				const FColor Out = FWRToolMesh::Pack(FLinearColor(FColor(28, 28, 32)) * 0.7f, 0.8f), Band = FWRToolMesh::Pack(b == 0 ? FLinearColor(FColor(230, 216, 192)) * 0.6f : FLinearColor(FColor(28, 28, 32)) * 0.7f, 0.5f);
				Sh.Tri(P0, P1, P2, b == 0 ? Band : Out, Z, Z, Z); Sh.Tri(P0, P2, P3, b == 0 ? Band : Out, Z, Z, Z);
				Sh.Tri(P0, P2, P1, b == 0 ? Band : Out, Z, Z, Z); Sh.Tri(P0, P3, P2, b == 0 ? Band : Out, Z, Z, Z);
			}
		}
		UProceduralMeshComponent* Shell = NewObject<UProceduralMeshComponent>(this, TEXT("GeodeShell"));
		Shell->SetupAttachment(RootComponent);
		Shell->RegisterComponent();
		Sh.ApplyTo(Shell);
		// The druse: a carpet of points on each half's open face.
		FWRToolMesh Pts;
		FRandomStream Rng(static_cast<int32>(GetTypeHash(GetName())));
		for (int32 i = 0; i < 46; ++i)
		{
			const float Side = i % 2 ? 1.f : -1.f;
			const float A = Rng.FRandRange(0.15f, PI - 0.15f);
			const FRotator Open(0.f, 0.f, Side * 28.f);
			const FVector Root = Open.RotateVector(FVector(FMath::Cos(A) * R * Rng.FRandRange(0.2f, 0.85f), Side * 10.f, FMath::Sin(A) * R * 0.75f * Rng.FRandRange(0.2f, 0.9f))) + FVector(0, 0, 6.f);
			const FVector Dir = Open.RotateVector(FVector(Rng.FRandRange(-0.3f, 0.3f), -Side, Rng.FRandRange(-0.2f, 0.4f))).GetSafeNormal();
			WRCrystalThingsImpl::CzPoint(Pts, Root, Dir, Rng.FRandRange(10.f, 26.f), 7.f, FLinearColor(FColor(74, 40, 120)), FLinearColor(FColor(150, 110, 220)), FLinearColor(FColor(214, 196, 255)), Rng.FRandRange(0.f, PI));
		}
		UProceduralMeshComponent* Druse = NewObject<UProceduralMeshComponent>(this, TEXT("GeodeDruse"));
		Druse->SetupAttachment(RootComponent);
		Druse->RegisterComponent();
		Pts.ApplyTo(Druse);
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, Druse);
			MID->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor(0.62f, 0.42f, 1.f));
			MID->SetScalarParameterValue(TEXT("GlowScale"), CtTn(TEXT("crystalWorld.glow.scale"), 6.f));
			MID->SetScalarParameterValue(TEXT("Rest"), 0.6f);
			Druse->SetMaterial(0, MID);
			if (UWRCrystalWorld* W = UWRCrystalWorld::Get(this)) W->RegisterGlow(MID, GetActorLocation());
		}
		Reach = 300.f;
	}
}

FString AWRCrystalSpot::Use(AWRPlayerController* PC)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!Me || !PS) return TEXT("Not now");
	if (FVector::Dist(Me->GetActorLocation(), GetActorLocation()) > Reach + 150.f) return TEXT("Too far");
	if (Kind == TEXT("keystone"))
	{
		Strike(Me);
		return FString();
	}
	const FString Name = PS->GetPlayerName();
	if (DoneBy.Contains(Name)) return TEXT("Nothing more here");
	UWRInventoryComponent* Inv = PS->GetInventory();
	if (!Inv) return TEXT("Not now");
	FString Got;
	for (const TPair<FName, int32>& G : Gives)
	{
		const int32 Added = Inv->AddItem(G.Key, G.Value);
		if (Added > 0) Got += FString::Printf(TEXT("%s%s x%d"), Got.IsEmpty() ? TEXT("") : TEXT(", "), *CzItemName(G.Key), Added);
	}
	if (Got.IsEmpty()) return TEXT("Your bag is full");
	DoneBy.Add(Name);
	// A singing geode is one hoard: the first to reach it takes it, and it closes behind them.
	if (Kind == TEXT("geode"))
	{
		WRSound::PlayAt(this, TEXT("crystal.ring_on"), GetActorLocation());
		SetLifeSpan(0.5f);
		return FString::Printf(TEXT("Took %s from the geode"), *Got);
	}
	WRSound::PlayAt(this, TEXT("crystal.pickup"), GetActorLocation());
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL %s took %s at %s"), *Name, *Got, *SpotId.ToString());
	return Lore.IsEmpty() ? FString::Printf(TEXT("Took %s"), *Got) : FString::Printf(TEXT("Took %s. %s"), *Got, *Lore);
}

void AWRCrystalSpot::Strike(AWRCharacterBase* By)
{
	if (!HasAuthority() || Kind != TEXT("keystone")) return;
	UWRCrystalWorld* W = UWRCrystalWorld::Get(this);
	if (W) W->StrikeKeystone(Hall, By, this);
}

// ================================================================================================ UWRCrystalWorld (the live half)

void UWRCrystalWorld::StrikeKeystone(int32 HallIndex, AWRCharacterBase* By, AWRCrystalSpot* Spot)
{
	UWorld* World = GetWorld();
	AWRCrystalState* S = State.Get();
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!World || !S || !D.Halls.IsValidIndex(HallIndex)) return;
	if (S->HallLitUntil.Num() < D.Halls.Num()) S->HallLitUntil.SetNumZeroed(D.Halls.Num());
	const float Now = float(CzGameHours(World));
	const bool bWasLit = S->HallLitUntil[HallIndex] > Now;
	// The hall holds lit for 20 game minutes (crystal 2.1); striking it again while lit starts the 20 over.
	S->HallLitUntil[HallIndex] = Now + CtTn(TEXT("crystalWorld.light.keystoneMinutes"), 20.f) / 60.f;
	S->ForceNetUpdate();
	const FVector At = Spot ? Spot->GetActorLocation() : D.Halls[HallIndex].Keystone;
	WRCrystal::Emit(World, At, WRCrystal::NoiseRadius(TEXT("chime")), By, TEXT("chime"));
	WRFx::Broadcast(World, TEXT("crystal.keystone"), At + FVector(0.f, 0.f, 280.f), FRotator::ZeroRotator, 1.f, TEXT("crystal.keystone"));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL keystone of %s struck by %s%s"), *D.Halls[HallIndex].Id.ToString(), *GetNameSafe(By), bWasLit ? TEXT(" (again)") : TEXT(""));
	// Saga Venn's "Listen first" counts halls, not strikes: each hall once per survivor.
	AWRPlayerState* PS = By ? By->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS && Spot && !Spot->StruckBy.Contains(PS->GetPlayerName()))
	{
		Spot->StruckBy.Add(PS->GetPlayerName());
		if (UWRWorldSystems* WS = World->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("keystone_struck"), 1);
	}
	if (By && !bWasLit) CtTell(By, FString::Printf(TEXT("%s rings, and the whole hall answers"), *D.Halls[HallIndex].Name));
}

void UWRCrystalWorld::Light(const FVector& At, float Radius, float Strength, const void* Key)
{
	if (!bActive || Radius <= 0.f || Strength <= 0.f) return;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (HallRings.Num() != D.Halls.Num() + 1) HallRings.SetNum(D.Halls.Num() + 1);
	// Which linings the sphere touches: every hall whose shape is within the radius (a tube between two halls lights
	// the ends of both), else the loose ring set (the surface's outcrops and the domes' bowls).
	TArray<int32> Shapes;
	D.ShapesIn(FBox(At - FVector(Radius), At + FVector(Radius)), Shapes);
	bool bAny = false;
	const auto Push = [&](int32 Slot)
	{
		TArray<FRing>& Rings = HallRings[Slot];
		int32 Use = INDEX_NONE;
		if (Key) for (int32 i = 0; i < Rings.Num(); ++i) if (Rings[i].Key == Key) { Use = i; break; }
		if (Use == INDEX_NONE && Rings.Num() < 4) Use = Rings.AddDefaulted();
		if (Use == INDEX_NONE)
		{
			// The weakest ring now gives way (a fresh shout beats a fading footstep).
			float Low = TNumericLimits<float>::Max();
			for (int32 i = 0; i < Rings.Num(); ++i) { const float L = Rings[i].Now(); if (L < Low) { Low = L; Use = i; } }
			if (Low > Strength) return;
		}
		Rings[Use] = { At, Radius, Strength, 0.f, Key };
	};
	for (const int32 Sh : Shapes)
	{
		if (Sh >= D.Halls.Num()) continue;
		if (D.ShapeAir(Sh, At) > Radius) continue;
		Push(Sh);
		bAny = true;
	}
	if (!bAny) Push(D.Halls.Num());
}

void UWRCrystalWorld::RegisterGlow(UMaterialInstanceDynamic* MID, const FVector& At)
{
	if (!MID) return;
	const FWRCrystalData::FHall* H = WRCrystal::HallAt(At);
	const int32 Hall = H ? int32(H - FWRCrystalData::Get().Halls.GetData()) : FWRCrystalData::Get().Halls.Num();
	ExtraGlows.Add({ MID, At, Hall });
}

void UWRCrystalWorld::StepLight(float Dt)
{
	UWorld* World = GetWorld();
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (HallRings.Num() != D.Halls.Num() + 1) HallRings.SetNum(D.Halls.Num() + 1);
	const float Fade = FMath::Max(0.5f, CtTn(TEXT("crystalWorld.light.fade"), 6.f));
	for (TArray<FRing>& Rings : HallRings)
	{
		for (int32 i = Rings.Num() - 1; i >= 0; --i)
		{
			Rings[i].Age += Dt / Fade;
			if (Rings[i].Age >= 1.f) Rings.RemoveAtSwap(i);
		}
	}
	// Footsteps, here on this machine: every character it sees near the view, four times a second.
	StepClock -= Dt;
	FVector View = FVector::ZeroVector;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		FRotator R;
		PC->GetPlayerViewPoint(View, R);
	}
	if (StepClock <= 0.f)
	{
		StepClock = 0.25f;
		const float Near = CtTn(TEXT("crystalWorld.light.stepRange"), 8000.f);
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive() || FVector::DistSquared(It->GetActorLocation(), View) > Near * Near) continue;
			const float R = WRCrystal::StepRadius(*It);
			if (R <= 0.f) continue;
			const FVector Feet = It->GetActorLocation() - FVector(0.f, 0.f, It->GetSimpleCollisionHalfHeight());
			if (!WRCrystal::InCave(Feet + FVector(0.f, 0.f, 60.f)) && !WRCrystal::Contains(Feet)) continue;
			Light(Feet, R, CzStrengthFor(R) * CtTn(TEXT("crystalWorld.light.stepScale"), 0.6f), *It);
		}
	}
	// The halls near the view: their rings, their rest (the drip song in rain lifts the Geode Halls'), their keystone
	// light (eased in and out), their lights at the densest clusters.
	AWRCrystalCaves* Caves = CavesActor.Get();
	const AWRCrystalState* S = State.IsValid() ? State.Get() : AWRCrystalState::Get(World);
	if (S && !State.IsValid()) State = const_cast<AWRCrystalState*>(S);
	const float Rest = CtTn(TEXT("crystalWorld.glow.rest"), 0.15f);
	const bool bRain = CzRaining(World);
	const float LitLevel = CtTn(TEXT("crystalWorld.light.keystoneLit"), 2.f);
	const float Range = CtTn(TEXT("crystalWorld.light.hallRange"), 16000.f);
	if (HallLit.Num() != D.Halls.Num()) HallLit.SetNumZeroed(D.Halls.Num());
	const auto Params = [&](UMaterialInstanceDynamic* MID, int32 Slot, float RestHere, float Lit)
	{
		static const FName Names[] = { TEXT("RingA"), TEXT("RingB"), TEXT("RingC"), TEXT("RingD") };
		FLinearColor Str(0.f, 0.f, 0.f, 0.f);
		const TArray<FRing>& Rings = HallRings[Slot];
		for (int32 i = 0; i < 4; ++i)
		{
			if (Rings.IsValidIndex(i))
			{
				MID->SetVectorParameterValue(Names[i], FLinearColor(Rings[i].At.X, Rings[i].At.Y, Rings[i].At.Z, Rings[i].Radius));
				Str.Component(i) = Rings[i].Now();
			}
			else MID->SetVectorParameterValue(Names[i], FLinearColor(0.f, 0.f, -1.0e7f, 100.f));
		}
		MID->SetVectorParameterValue(TEXT("RingS"), Str);
		MID->SetScalarParameterValue(TEXT("Rest"), RestHere);
		MID->SetScalarParameterValue(TEXT("Lit"), Lit);
	};
	for (int32 h = 0; h < D.Halls.Num(); ++h)
	{
		const FWRCrystalData::FHall& H = D.Halls[h];
		const bool bNear = FVector::DistSquared(H.Centre(), View) < FMath::Square(Range + FMath::Max(H.Rx, H.Ry));
		const float Want = S && S->IsHallLit(h) ? LitLevel : 0.f;
		HallLit[h] = FMath::FInterpTo(HallLit[h], Want + ResonanceGlow, Dt, Want > HallLit[h] ? 3.f : 0.4f);
		const float RestHere = Rest + (bRain && H.Zone == TEXT("C2") && H.At.Z > CtTn(TEXT("crystalWorld.light.dripAbove"), -500.f) ? CtTn(TEXT("crystalWorld.light.rainRest"), 0.6f) - Rest : 0.f);
		if (!bNear) { if (TArray<TWeakObjectPtr<UPointLightComponent>>* L = HallLights.Find(h)) for (const TWeakObjectPtr<UPointLightComponent>& P : *L) if (P.IsValid() && P->IsVisible()) P->SetVisibility(false); continue; }
		if (UMaterialInstanceDynamic* MID = Caves ? Caves->HallGlow(h) : nullptr) Params(MID, h, RestHere, HallLit[h]);
		// The hall's lights stand at its densest clusters and follow its level: rest + lit + the strongest ring there.
		const TArray<FVector>* Cl = Caves ? Caves->HallClusters(h) : nullptr;
		if (!Cl || Cl->Num() == 0) continue;
		TArray<TWeakObjectPtr<UPointLightComponent>>& Lights = HallLights.FindOrAdd(h);
		if (Lights.Num() == 0 && Caves)
		{
			const FLinearColor Col = H.Zone == TEXT("C3") ? FLinearColor(FColor(236, 232, 255)) : FLinearColor(FColor(196, 176, 255));
			for (int32 i = 0; i < FMath::Min(3, Cl->Num()); ++i)
			{
				UPointLightComponent* P = NewObject<UPointLightComponent>(Caves);
				P->SetupAttachment(Caves->GetRootComponent());
				P->SetWorldLocation((*Cl)[i]);
				P->SetLightColor(Col);
				P->SetAttenuationRadius(FMath::Max(H.Rx, H.Ry) * CtTn(TEXT("crystalWorld.light.reach"), 1.1f));
				P->SetCastShadows(false);
				P->SetIntensity(0.f);
				P->RegisterComponent();
				Lights.Add(P);
			}
		}
		for (const TWeakObjectPtr<UPointLightComponent>& WP : Lights)
		{
			UPointLightComponent* P = WP.Get();
			if (!P) continue;
			float Ring = 0.f;
			for (const FRing& R : HallRings[h])
			{
				const float Dd = FVector::Dist(R.At, P->GetComponentLocation());
				Ring = FMath::Max(Ring, R.Now() * FMath::Square(FMath::Clamp(1.f - Dd / FMath::Max(R.Radius, 1.f), 0.f, 1.f)));
			}
			const float Level = RestHere * 0.3f + HallLit[h] + Ring;
			P->SetIntensity(Level * CtTn(TEXT("crystalWorld.light.power"), 900.f));
			const bool bOn = Level > 0.03f;
			if (P->IsVisible() != bOn) P->SetVisibility(bOn);
		}
	}
	// The nodes' crystals and the surface outcrops follow their hall (or the loose rings out on the plateau).
	for (int32 i = ExtraGlows.Num() - 1; i >= 0; --i)
	{
		UMaterialInstanceDynamic* MID = ExtraGlows[i].MID.Get();
		if (!MID) { ExtraGlows.RemoveAtSwap(i); continue; }
		if (FVector::DistSquared(ExtraGlows[i].At, View) > Range * Range) continue;
		const int32 Slot = ExtraGlows[i].Hall;
		Params(MID, Slot, Rest, D.Halls.IsValidIndex(Slot) ? HallLit[Slot] : ResonanceGlow);
	}
}

void UWRCrystalWorld::StepServerNoise(float Dt)
{
	// The server's own hearing of footsteps and landings (listeners only: no traffic), four times a second.
	ServerStepClock -= Dt;
	if (ServerStepClock > 0.f) return;
	ServerStepClock = 0.25f;
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive()) continue;
		const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
		if (!WRCrystal::Contains(Feet)) { FallStart.Remove(C); continue; }
		const UCharacterMovementComponent* M = C->GetCharacterMovement();
		const bool bFalling = M && M->IsFalling();
		if (bFalling) { if (!FallStart.Contains(C)) FallStart.Add(C, Now); continue; }
		if (const double* Began = FallStart.Find(C))
		{
			// A landing from a real jump or drop (crystal 2.1: 6 m), not a step down a stair.
			if (Now - *Began > CtTn(TEXT("crystalWorld.noise.landAfter"), 0.4f)) WRCrystal::Emit(World, Feet, WRCrystal::NoiseRadius(TEXT("land")), C, TEXT("land"));
			FallStart.Remove(C);
		}
		const float R = WRCrystal::StepRadius(C);
		if (R > 0.f) WRCrystal::Emit(World, Feet, R, C, TEXT("step"));
	}
	for (auto It = FallStart.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	StepQuietRoad();
}

void UWRCrystalWorld::ProjectileLanded(const UObject* WorldContext, const FVector& At, AActor* By, FName Kind, bool bQuiet)
{
	UWRCrystalWorld* W = Get(WorldContext);
	if (!W || !W->bActive || !WRCrystal::Contains(At)) return;
	// A thrown stone clatters 15 m round where it lands; an arrow ticks off the rock (half that from a Glassleg bow).
	const float R = WRCrystal::NoiseRadius(Kind == TEXT("riftstone") ? TEXT("stone") : TEXT("arrow")) * (bQuiet ? 0.5f : 1.f);
	WRCrystal::Emit(WorldContext, At, R, By, TEXT("land_shot"));
	// A shot that lands on a keystone chime strikes it.
	for (const TWeakObjectPtr<AWRCrystalSpot>& Sp : W->Spots)
	{
		AWRCrystalSpot* S = Sp.Get();
		if (S && S->Kind == TEXT("keystone") && FVector::Dist(S->GetActorLocation() + FVector(0.f, 0.f, 280.f), At) < CtTn(TEXT("crystalWorld.keystoneShotReach"), 220.f))
		{
			S->Strike(Cast<AWRCharacterBase>(By));
			break;
		}
	}
}

AWRCrystalSpot* UWRCrystalWorld::SpotNear(const FVector& At) const
{
	AWRCrystalSpot* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AWRCrystalSpot>& Sp : Spots)
	{
		AWRCrystalSpot* S = Sp.Get();
		if (!S) continue;
		const float Dd = FVector::Dist(S->GetActorLocation(), At);
		if (Dd < S->Reach && Dd < BestD) { BestD = Dd; Best = S; }
	}
	return Best;
}

bool UWRCrystalWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWRCrystalWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	const AWRCrystalSpot* S = W->SpotNear(Me->GetActorLocation());
	if (!S) return false;
	OutTarget = S->Title;
	OutVerb = S->Verb;
	if (S->Kind == TEXT("keystone"))
	{
		const AWRCrystalState* St = AWRCrystalState::Get(PC);
		if (St && St->IsHallLit(S->Hall)) OutVerb = TEXT("Ringing: strike again to hold the light");
	}
	return true;
}

bool UWRCrystalWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWRCrystalWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	const AWRCrystalSpot* S = W->SpotNear(Me->GetActorLocation());
	if (!S) return false;
	PC->SendCommand(TEXT("world crystal spot"));
	return true;
}

FString UWRCrystalWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRCrystalWorld* W = Get(PC);
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!W || !W->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("spot"))
	{
		if (AWRCrystalSpot* S = W->SpotNear(Me->GetActorLocation())) return S->Use(PC);
		return TEXT("Nothing here");
	}
	return TEXT("world crystal spot");
}

// ================================================================================================ the server's things

void UWRCrystalWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRCrystalData& D = FWRCrystalData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRCrystalState* S = World->SpawnActor<AWRCrystalState>(AWRCrystalState::StaticClass(), FTransform::Identity, SP);
	if (S) S->HallLitUntil.SetNumZeroed(D.Halls.Num());
	State = S;

	// ---- the gathering (crystal 2.6-2.7; numbers in tuning crystalWorld.nodes.<kind>) ----
	struct FKindRow { const TCHAR* Key; const TCHAR* Item; };
	static const FKindRow Kinds[] = {
		{ TEXT("resonant"), TEXT("resonant_crystal") }, { TEXT("dust"), TEXT("crystal_dust") }, { TEXT("selenite"), TEXT("selenite") },
		{ TEXT("spar"), TEXT("doubling_spar") }, { TEXT("amethyst"), TEXT("gem_amethyst") }, { TEXT("saltpetre"), TEXT("saltpetre") },
		{ TEXT("ochre"), TEXT("ochre") }, { TEXT("wax"), TEXT("wax") }, { TEXT("copper"), TEXT("copper_ore") }, { TEXT("tin"), TEXT("tin_ore") },
		{ TEXT("iron"), TEXT("iron_ore") }, { TEXT("coal"), TEXT("charcoal") }, { TEXT("silver"), TEXT("silver_ore") }, { TEXT("star"), TEXT("star_iron_ore") },
		{ TEXT("gems"), TEXT("gem_ruby") }, { TEXT("rhubarb"), TEXT("candle_rhubarb") }, { TEXT("glowmelon"), TEXT("glowmelon") } };
	int32 Made = 0;
	for (const FKindRow& K : Kinds)
	{
		const TArray<FWRCrystalData::FNode>* List = D.Resources.Find(FName(K.Key));
		if (!List) continue;
		const FString T = FString::Printf(TEXT("crystalWorld.nodes.%s."), K.Key);
		for (const FWRCrystalData::FNode& N : *List)
		{
			// A gem cluster in band 3 gives a ruby, a topaz or an onyx by its place (the same on every run).
			FName Item(K.Item);
			if (FName(K.Key) == TEXT("gems")) { static const TCHAR* G[] = { TEXT("gem_ruby"), TEXT("gem_topaz"), TEXT("gem_onyx") }; Item = G[FMath::Abs(int32(N.At.X / 100.f) + int32(N.At.Y / 100.f)) % 3]; }
			const FTransform Tr(FRotator(0.f, float((Made * 137) % 360), 0.f), N.At);
			AWRResourceNode* Node = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Node) continue;
			Node->ItemId = Item;
			Node->Hits = FMath::Max(1, int32(CtTn(T + TEXT("hits"), 4.f)));
			Node->PerHit = FMath::Max(1, FMath::RoundToInt(CtTn(T + TEXT("perHit"), 1.f)));
			Node->RespawnSeconds = CtTn(T + TEXT("respawn"), 1800.f);
			Node->GemChance = CtTn(T + TEXT("gemChance"), 0.04f);
			Node->FinishSpawning(Tr);
			++Made;
			TWeakObjectPtr<AWRResourceNode> Weak(Node);
			if (Item == TEXT("resonant_crystal"))
			{
				// Pick on the ring (crystal 2.6): each strike rings the node 1.2 s; a strike while it still rings gives one
				// more. Every strike is a 20 m noise. The Resonance pays x3; after rain, +50% (the plenty).
				TSharedRef<double> LastStrike = MakeShared<double>(-100.0);
				TSharedRef<bool> bOnRing = MakeShared<bool>(false);
				Node->YieldScale = [Weak, LastStrike, bOnRing](AWRCharacterBase* Who)
				{
					const AWRResourceNode* Nd = Weak.Get();
					if (!Nd) return 1.f;
					const double Now = Nd->GetWorld()->GetTimeSeconds();
					*bOnRing = Now - *LastStrike <= CtTn(TEXT("crystalWorld.ring.seconds"), 1.2f);
					float Mult = *bOnRing ? 2.f : 1.f;
					if (const UWRCrystalWorld* CW = UWRCrystalWorld::Get(Nd)) Mult *= CW->SignatureScale();
					return Mult;
				};
				Node->OnHarvested = [Weak, LastStrike, bOnRing](AWRCharacterBase* Who)
				{
					AWRResourceNode* Nd = Weak.Get();
					if (!Nd) return;
					*LastStrike = Nd->GetWorld()->GetTimeSeconds();
					const FVector At = Nd->GetActorLocation() + FVector(0.f, 0.f, 90.f);
					WRFx::Broadcast(Nd, *bOnRing ? TEXT("crystal.ring_on") : TEXT("crystal.ring"), At, FRotator::ZeroRotator, 1.f, *bOnRing ? TEXT("crystal.ring_on") : TEXT("crystal.ring"));
					WRCrystal::Emit(Nd, At, WRCrystal::NoiseRadius(TEXT("hitCrystal")), Who, TEXT("crystal"));
					if (*bOnRing)
					{
						AWRPlayerState* PS = Who ? Who->GetPlayerState<AWRPlayerState>() : nullptr;
						if (PS) if (UWRWorldSystems* WS = Nd->GetWorld()->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("ring_mined"), 1);
					}
				};
			}
			else if (Item == TEXT("doubling_spar") || Item == TEXT("gem_amethyst") || Item == TEXT("selenite"))
			{
				// Any crystal rings when struck (a 20 m noise: crystal 2.1), even the ones you don't pick on the ring.
				Node->OnHarvested = [Weak](AWRCharacterBase* Who)
				{
					AWRResourceNode* Nd = Weak.Get();
					if (!Nd) return;
					const FVector At = Nd->GetActorLocation() + FVector(0.f, 0.f, 60.f);
					WRFx::Broadcast(Nd, TEXT("crystal.ring"), At, FRotator::ZeroRotator, 0.7f, TEXT("crystal.ring"));
					WRCrystal::Emit(Nd, At, WRCrystal::NoiseRadius(TEXT("hitCrystal")), Who, TEXT("crystal"));
				};
			}
			else if (Item == TEXT("candle_rhubarb") || Item == TEXT("glowmelon"))
			{
				// The wild crops (crystal 2.7; survival-crafting): their seeds come from gathering them.
				const FName Seeds(*(Item.ToString() + TEXT("_seeds")));
				Node->OnHarvested = [Seeds](AWRCharacterBase* Who)
				{
					if (FMath::FRand() < CtTn(TEXT("crystalWorld.seedChance"), 0.35f)) if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(Seeds, 1);
				};
			}
			else
			{
				// A pick on stone and ore still clangs (a creature-hit's 12 m); a hand at dust, ochre, wax or a crop is quiet.
				const bool bLoud = FWRGathering::ToolKindFor(Item) == TEXT("pick");
				if (bLoud) Node->OnHarvested = [Weak](AWRCharacterBase* Who)
				{
					if (AWRResourceNode* Nd = Weak.Get()) WRCrystal::Emit(Nd, Nd->GetActorLocation() + FVector(0.f, 0.f, 60.f), WRCrystal::NoiseRadius(TEXT("hitCreature")), Who, TEXT("pick"));
				};
			}
			Nodes.Add(Node);
		}
	}

	// ---- keystone chimes: one per hall that has one (crystal 2.1), under its densest cluster ----
	const auto MakeSpot = [&](FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb) -> AWRCrystalSpot*
	{
		const FTransform Tr(FRotator::ZeroRotator, At);
		AWRCrystalSpot* Sp = World->SpawnActorDeferred<AWRCrystalSpot>(AWRCrystalSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Sp) return nullptr;
		Sp->Kind = Kind;
		Sp->SpotId = Id;
		Sp->Title = Title;
		Sp->Verb = Verb;
		Sp->FinishSpawning(Tr);
		Spots.Add(Sp);
		return Sp;
	};
	int32 Keys = 0;
	for (int32 h = 0; h < D.Halls.Num(); ++h)
	{
		const FWRCrystalData::FHall& H = D.Halls[h];
		if (!H.bKeystone) continue;
		if (AWRCrystalSpot* Sp = MakeSpot(TEXT("keystone"), H.Id, H.Keystone + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("Keystone chime · %s"), *H.Name), TEXT("Strike it")))
		{
			Sp->Hall = h;
			Sp->Reach = 320.f;
			++Keys;
		}
	}

	// ---- keepsakes: the seven lore items where their story is (crystal 2.20) ----
	for (const FWRCrystalData::FLore& L : D.Lore)
	{
		const FWRCrystalData::FSite* Site = D.FindSite(L.Site);
		if (!Site) continue;
		const FVector Off(CtTn(FString::Printf(TEXT("crystalWorld.keepsakeAt.%s.x"), *L.Site.ToString()), 250.f), CtTn(FString::Printf(TEXT("crystalWorld.keepsakeAt.%s.y"), *L.Site.ToString()), 0.f), 40.f);
		FVector At = Site->At + FRotator(0.f, Site->Yaw, 0.f).RotateVector(Off);
		if (!Site->bCave) { float Z = 0.f; if (WRLand::GroundUnder(At + FVector(0.f, 0.f, 3000.f), Z)) At.Z = Z + 40.f; }
		if (AWRCrystalSpot* Sp = MakeSpot(TEXT("keepsake"), L.Item, At, CzItemName(L.Item), TEXT("Take")))
		{
			Sp->Gives.Add(L.Item, 1);
		}
	}
	// ---- found things (crystal 2.19.2-2.19.3): the Wardcarver compass at the Dropped Hammer (S3), the three found runes
	// in the Ward Chambers' sealed rooms, each behind its ward (once per survivor) ----
	const auto Find = [&](FName Id, const FVector& At, FName Item, const FString& Lore)
	{
		if (AWRCrystalSpot* Sp = MakeSpot(TEXT("find"), Id, At, CzItemName(Item), TEXT("Take")))
		{
			Sp->Gives.Add(Item, 1);
			Sp->Lore = Lore;
		}
	};
	if (const FWRCrystalData::FSite* S3 = D.FindSite(TEXT("S3")))
	{
		FVector At = S3->At + FVector(CtTn(TEXT("crystalWorld.compassAt.x"), -420.f), CtTn(TEXT("crystalWorld.compassAt.y"), 260.f), 0.f);
		float Z = 0.f;
		if (WRLand::GroundUnder(At + FVector(0.f, 0.f, 3000.f), Z)) At.Z = Z + 30.f;
		Find(TEXT("compass"), At, TEXT("wardcarver_compass"), TEXT("Its needle swings west, toward the gorge, and won't settle."));
	}
	const auto HallFloor = [&](FName Id, const FVector& Off) { const FWRCrystalData::FHall* H = D.FindHall(Id); return H ? H->At + FRotator(0.f, H->Yaw, 0.f).RotateVector(Off) + FVector(0.f, 0.f, 40.f) : FVector::ZeroVector; };
	if (D.FindHall(TEXT("A12"))) Find(TEXT("rune_prism"), HallFloor(TEXT("A12"), FVector::ZeroVector), TEXT("rune_prism"), TEXT("Set in a lead tray with a hexafoil pressed into the lid."));
	if (D.FindHall(TEXT("A13"))) Find(TEXT("rune_warding"), HallFloor(TEXT("A13"), FVector::ZeroVector), TEXT("rune_warding"), TEXT("Wrapped in waxed linen, with a second, empty wrapping beside it."));
	if (D.FindHall(TEXT("W3"))) Find(TEXT("rune_doubling"), HallFloor(TEXT("W3"), FVector(-250.f, 180.f, 60.f)), TEXT("rune_doubling"), TEXT("On the lathe's bed among the lens grindings: a rune, and its reflection scratched beside it."));

	// The Silent Vault's pay (crystal 2.5): V3 the Archive of Forks (three ward forks), V6 the Last Carver (the Carver's
	// Chisel in her hand, and the Rune of Silence: only here, once per survivor).
	if (const FWRCrystalData::FSite* V3 = D.FindSite(TEXT("V3")))
	{
		if (AWRCrystalSpot* Sp = MakeSpot(TEXT("find"), TEXT("vault_forks"), V3->At + FVector(CtTn(TEXT("crystalWorld.vaultAt.V3.x"), 280.f), 0.f, 60.f), TEXT("A rack of ward forks"), TEXT("Take three")))
		{
			Sp->Gives.Add(TEXT("ward_fork"), 3);
			Sp->Lore = TEXT("Hundreds of forks in racks to the ceiling, each tuned and labelled with a ward's name. Three still ring.");
		}
	}
	if (const FWRCrystalData::FSite* V6 = D.FindSite(TEXT("V6")))
	{
		if (AWRCrystalSpot* Sp = MakeSpot(TEXT("find"), TEXT("last_carver"), V6->At + FVector(CtTn(TEXT("crystalWorld.vaultAt.V6.x"), 220.f), 0.f, 60.f), TEXT("The Last Carver's hand"), TEXT("Take the chisel")))
		{
			Sp->Gives.Add(TEXT("carvers_chisel"), 1);
			Sp->Gives.Add(TEXT("rune_silence"), 1);
			Sp->Lore = TEXT("Her hexafoil is one petal short. The chisel was still in her hand.");
		}
	}

	// ---- the chime junction caches behind their wards (crystal 2.4: kit chime_cache) ----
	int32 Caches = 0;
	for (const FWRCrystalData::FKit& K : D.Kit)
	{
		if (K.Kind != TEXT("chime_cache")) continue;
		const FWRCrystalData::FHall* H = D.FindHall(K.Hall);
		const FVector At = H ? H->At + FVector(0.f, 0.f, 50.f) : K.At;
		const FTransform Tr(FRotator::ZeroRotator, At);
		AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) continue;
		C->Kind = TEXT("chest");
		C->Biome = WRCrystal::Biome();
		C->Title = TEXT("A Wardcarver's chime cache");
		C->Items = { { TEXT("resonant_crystal"), 4 }, { TEXT("gem_amethyst"), 1 }, { TEXT("wax"), 3 }, { TEXT("ochre"), 2 } };
		C->Lore = TEXT("A lead box under the junction's chime stone. Wax for the ears, ochre for the marks, and crystal for the rest.");
		C->RefillHours = CtTn(TEXT("crystalWorld.cacheRefillHours"), 48.f);
		C->FinishSpawning(Tr);
		++Caches;
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL server: %d nodes, %d keystone chimes, %d spots, %d caches"), Made, Keys, Spots.Num(), Caches);
}

float UWRCrystalWorld::SignatureScale() const
{
	// The plenty after rain (crystal 2.9: +50% for the next block) and the Resonance's x3 (2.10).
	float M = 1.f;
	if (UWorld* World = GetWorld())
	{
		if (CzRaining(World)) RainSeenAt = World->GetTimeSeconds();
		if (RainSeenAt > 0.0 && World->GetTimeSeconds() - RainSeenAt < CtTn(TEXT("crystalWorld.plentySeconds"), 1200.f)) M *= CtTn(TEXT("crystalWorld.plentyYield"), 1.5f);
	}
	return M * ResonanceYield;
}

void UWRCrystalWorld::StepVenn(float Dt)
{
	// Saga Venn (crystal 2.11): by day she carves a keystone plate at the Great Shrine (world.json "at"); at night she
	// sits at the hexafoil's edge striking a fork and listening. She moves only when nobody is near (no one sees her blink).
	VennClock -= Dt;
	if (VennClock > 0.f) return;
	VennClock = 5.f;
	UWorld* World = GetWorld();
	const int32 Phase = AWRGameState::IsNightIn(World) ? 1 : 0;
	if (Phase == VennPhase) return;
	AWRWorldNpc* Venn = nullptr;
	for (TActorIterator<AWRWorldNpc> It(World); It; ++It) if (It->NpcRole == TEXT("master") && It->NpcId == TEXT("saga_venn")) Venn = *It;
	if (!Venn) return;
	if (VennHome.IsZero()) { VennHome = Venn->GetActorLocation(); VennFace = Venn->GetActorRotation(); }
	const FWRCrystalData::FShrine* Sh = nullptr;
	for (const FWRCrystalData::FShrine& S : FWRCrystalData::Get().Shrines) if (S.bGreat) Sh = &S;
	if (!Sh) return;
	FVector Spot = VennHome;
	FRotator Face = VennFace;
	if (Phase == 1)
	{
		// The hexafoil's edge (8 m across: she sits at 4.2 m from its middle, facing in, so she can feel the hum).
		const float Edge = CtTn(TEXT("crystalWorld.venn.nightRadius"), 420.f);
		const FVector Dir = FRotator(0.f, CtTn(TEXT("crystalWorld.venn.nightYaw"), 230.f), 0.f).Vector();
		Spot = FVector(Sh->At.X, Sh->At.Y, Sh->At.Z + 100.f) + Dir * Edge;
		Face = (-Dir).Rotation();
	}
	const float Unseen = CtTn(TEXT("crystalWorld.venn.unseen"), 3000.f);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && (FVector::Dist(P->GetActorLocation(), Venn->GetActorLocation()) < Unseen || FVector::Dist(P->GetActorLocation(), Spot) < Unseen)) return;
	}
	VennPhase = Phase;
	Venn->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
	Venn->SetActorRotation(Face);
	Venn->WorkAt = Spot;
	Venn->WorkFacing = Face;
	Venn->WorkSound = Phase == 1 ? FName(TEXT("crystal.fork")) : FName(TEXT("world.anvil_soft"));
	Venn->WorkEvery = Phase == 1 ? CtTn(TEXT("crystalWorld.venn.forkEvery"), 9.f) : 3.5f;
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Saga Venn goes to %s"), Phase == 1 ? TEXT("the hexafoil's edge") : TEXT("her bench"));
}

// ================================================================================================ the nodes' crystals

void WRCrystal::NodeLook(AActor* Node, FName Item, uint32 Seed)
{
	// The crystal of a node (crystal 2.6-2.7), modelled in code like the caves' linings: resonant crystal a violet
	// cluster 1-3 m off a basalt foot, its points paler toward the tips with a pale-blue band where the phantom sits;
	// doubling spar water-clear rhombs; amethyst a small deep-violet druse; selenite white stubs with grey growth bands.
	// All in M_CrystalGlow, so they light by sound like the halls (UWRCrystalWorld::RegisterGlow).
	if (!Node || Node->GetNetMode() == NM_DedicatedServer) return;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Node, TEXT("WRCrystalPoints"));
	P->SetupAttachment(Node->GetRootComponent());
	P->SetUsingAbsoluteScale(true);
	P->SetWorldScale3D(FVector::OneVector);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->RegisterComponent();
	FRandomStream Rng(static_cast<int32>(Seed));
	FWRToolMesh M;
	FLinearColor Base, Mid, Tip, Glow;
	int32 Count = 7;
	float LenLo = 60.f, LenHi = 180.f, Thick = 0.32f;
	if (Item == TEXT("resonant_crystal"))
	{
		Base = FLinearColor(FColor(74, 40, 120)); Mid = FLinearColor(FColor(150, 170, 230)); Tip = FLinearColor(FColor(214, 196, 255));
		Glow = FLinearColor(0.62f, 0.42f, 1.f); Count = 9 + int32(Seed % 6); LenLo = 90.f; LenHi = 290.f;
	}
	else if (Item == TEXT("gem_amethyst"))
	{
		Base = FLinearColor(FColor(60, 24, 96)); Mid = FLinearColor(FColor(110, 50, 170)); Tip = FLinearColor(FColor(170, 120, 230));
		Glow = FLinearColor(0.55f, 0.3f, 1.f); Count = 18; LenLo = 18.f; LenHi = 48.f; Thick = 0.4f;
	}
	else if (Item == TEXT("selenite"))
	{
		Base = FLinearColor(FColor(170, 168, 160)); Mid = FLinearColor(FColor(242, 240, 232)); Tip = Mid;
		Glow = FLinearColor(0.95f, 0.93f, 1.f); Count = 5; LenLo = 50.f; LenHi = 120.f; Thick = 0.45f;
	}
	else  // doubling spar
	{
		Base = FLinearColor(FColor(200, 214, 220)); Mid = FLinearColor(FColor(236, 244, 248)); Tip = Mid;
		Glow = FLinearColor(0.85f, 0.95f, 1.f); Count = 6; LenLo = 30.f; LenHi = 70.f; Thick = 0.9f;
	}
	const float Up = Item == TEXT("resonant_crystal") ? 30.f : 15.f;
	for (int32 i = 0; i < Count; ++i)
	{
		// Clusters grow from a common root and fan out (real druse): the tallest in the middle, the rest leaning out.
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float Lean = i == 0 ? Rng.FRandRange(0.f, 8.f) : Rng.FRandRange(14.f, 48.f);
		const FVector Dir = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Lean)).RotateVector(FVector::UpVector);
		const float Len = (i == 0 ? LenHi : Rng.FRandRange(LenLo, LenHi * 0.8f));
		const FVector Root(FMath::Cos(A) * Rng.FRandRange(0.f, 35.f), FMath::Sin(A) * Rng.FRandRange(0.f, 35.f), Up - 10.f);
		WRCrystalThingsImpl::CzPoint(M, Root, Dir, Len, Len * Thick, Base, Mid, Tip, Rng.FRandRange(0.f, PI));
	}
	M.ApplyTo(P);
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow"));
	if (!Mat) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, P);
	MID->SetVectorParameterValue(TEXT("GlowColor"), Glow);
	MID->SetScalarParameterValue(TEXT("GlowScale"), CtTn(TEXT("crystalWorld.glow.scale"), 6.f));
	MID->SetScalarParameterValue(TEXT("Rest"), CtTn(TEXT("crystalWorld.glow.rest"), 0.15f));
	P->SetMaterial(0, MID);
	if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Node)) W->RegisterGlow(MID, Node->GetActorLocation());
}
