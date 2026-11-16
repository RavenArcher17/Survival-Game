// The Storm peaks bosses and their arenas (cooks/biome-lightning.md 2.10-2.11). See WRStormBosses.h.
#include "WRStormBosses.h"
#include "WRStormCreatures.h"
#include "WRBoss.h"
#include "WRAltar.h"
#include "WRArtifacts.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

/** A grabbed body falling to the slab: its landing hurts (the drop is the damage). */
struct FWRStormBossDrop
	{
		TWeakObjectPtr<AWRCharacterBase> Who;
		double Since = 0.0;
		float FromZ = 0.f;
	};

	/** Server: one storm boss's fight. */
struct FWRStormBossState
	{
		bool bBleed = false, bCurse = false, bRime = false;
		double StuckUntil = 0.0, DropUntil = 0.0, CursedUntil = 0.0;
		// The Bellwether.
		TArray<TWeakObjectPtr<AWRCreature>> Herd;
		bool bHerdMade = false, bHerdCalled = false;
		double ChargeReadyAt = 0.0, RestUntil = 0.0, LastFold = -10000.0;
		float BellHP = 0.f, NoseHP = 0.f;
		// Galvara.
		TArray<TWeakObjectPtr<AWRCreature>> Cores, Kites;
		double NextRainAt = 0.0, GroundSince = 0.0, RetakeAt = 0.0;
		TArray<TPair<double, float>> StanceLog;
		FVector LashFrom = FVector::ZeroVector, LashTo = FVector::ZeroVector;
		// Coilback.
		bool bUnder = false;
		double SurfaceBy = 0.0, FrostUntil = 0.0, BlindUntil = 0.0;
		int32 Rings = 0, RingsStarted = 0;
		double RingStart = 0.0;
		FVector RingAt = FVector::ZeroVector, WanderTo = FVector::ZeroVector;
		// Kharvox.
		float Charge = 0.f;
		double NoGainUntil = 0.0, FlightClock = 0.0, NextCalledAt = 0.0, EyeTurnAt = 0.0;
		bool bArmed = false;
		int32 CalledTurn = 0;
		FVector EyeAt = FVector::ZeroVector, EyeDir = FVector(1.f, 0.f, 0.f);
		TWeakObjectPtr<AWRCharacterBase> Carried;
		double CarryStart = 0.0;
		FVector CarryFrom = FVector::ZeroVector;
		float CarryUp = 1500.f, HeadStance = 0.f;
		TArray<FWRStormBossDrop> Drops;
		int32 Stage = 0;
		double StageAt = 0.0;
		FVector Aim = FVector::ZeroVector, Aim2 = FVector::ZeroVector;
		bool bDoubleRing = false, bImpacted = false, bSecondFired = false;
		double ImpactAt = 0.0;
		TArray<TPair<TWeakObjectPtr<AWRStormProp>, TWeakObjectPtr<AWRStormProp>>> Pairs;
		TWeakObjectPtr<AActor> Shadow;
	};

namespace WRStormBossDetail
{
	TMap<TWeakObjectPtr<const AWRBoss>, FWRStormBossState>& SbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRStormBossState> Map;
		return Map;
	}

	FWRStormBossState& SbS(const AWRBoss* Boss) { return SbStates().FindOrAdd(Boss); }
	const FWRStormBossState* SbFind(const AWRBoss* Boss) { return SbStates().Find(Boss); }

	float SbScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	FVector SbGround(const UWorld* World, const FVector& At, bool& bOk, float Up = 3000.f, float Down = 8000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRStormBossGround), false);
		bOk = World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q);
		return bOk ? Hit.ImpactPoint : At;
	}

	/** Data/storm_peaks.json (read once). */
	TSharedPtr<FJsonObject> SbPeaks()
	{
		static TSharedPtr<FJsonObject> Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			Root = FWRTuning::LoadDataFile(TEXT("storm_peaks.json"));
		}
		return Root;
	}

	TSharedPtr<FJsonObject> SbArena(FName Id)
	{
		const TSharedPtr<FJsonObject> Root = SbPeaks();
		const TSharedPtr<FJsonObject>* Arenas = nullptr;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("arenas"), Arenas) && (*Arenas)->TryGetObjectField(Id.ToString(), Row)) return *Row;
		return nullptr;
	}

	/** A point in metres ([x, y] or [x, y, z], or {x, y, z}) to world cm (world cm = metres x 100). */
	bool SbMetres(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 2)
		{
			Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0) * 100.0;
			return true;
		}
		return false;
	}

	FVector SbCentre(const TSharedPtr<FJsonObject>& O)
	{
		return O.IsValid() ? FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->HasField(TEXT("z")) ? O->GetNumberField(TEXT("z")) : 0.0) * 100.0 : FVector::ZeroVector;
	}

	/** Every survivor and tame it may fight inside its chase range. */
	TArray<AWRCharacterBase*> SbFoes(const AWRBoss* Boss, float Range, bool bPlayersOnly = false)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || It->GetAttachParentActor()) continue;
			if (bPlayersOnly && !It->GetPlayerState()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) > Range) continue;
			Out.Add(*It);
		}
		return Out;
	}

	FVector SbFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	/** Distance from P to the segment A-B across the ground. */
	float SbSegDist2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D AB(B - A), AP(P - A);
		const float T = FMath::Clamp(FVector2D::DotProduct(AP, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
		return FVector2D::Distance(FVector2D(A) + AB * T, FVector2D(P));
	}

	/** A straight line of effect between two points (a bolt, a leader, an arc): a line recipe scaled to the length. */
	void SbLine(const UObject* W, FName Effect, const FVector& From, const FVector& To, FName Sound = NAME_None)
	{
		const FVector D = To - From;
		if (D.SizeSquared() < 1.f) return;
		WRFx::Broadcast(W, Effect, From, D.Rotation(), D.Size() / 100.f, Sound);
	}

	TArray<AWRStormProp*> SbRods(const AWRBoss* Boss, bool bStanding)
	{
		TArray<AWRStormProp*> Out;
		for (TActorIterator<AWRStormProp> It(Boss->GetWorld()); It; ++It)
		{
			if (It->Kind == TEXT("rim_rod") && FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) < 6000.f && (!bStanding || It->IsStanding())) Out.Add(*It);
		}
		return Out;
	}

	float SbNow(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }

	FName SbKind(const AWRBoss* Boss)
	{
		return Boss->CreatureId == TEXT("bellwether") || Boss->CreatureId == TEXT("galvara") || Boss->CreatureId == TEXT("coilback") || Boss->CreatureId == TEXT("kharvox") ? Boss->CreatureId : NAME_None;
	}

	/** A blow from one of its moves that isn't its row's own number (a dive's ring, the bone drop's fall). */
	void SbHitWith(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Shock, bool bUnblockable, bool bLow, uint8 Impact)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * SbScale() * (Boss->Phase >= 3 ? WRTune(TEXT("bosses.kharvox.phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = 80.f;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bHeavy = true;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = Boss->GetActorLocation();
		Hit.Element = Shock > 0.f ? WRTags::Element_Lightning : WRTags::Element_Physical;
		if (Shock > 0.f) Hit.Buildups.Add(TEXT("shock"), Shock);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
}
using namespace WRStormBossDetail;

// ---------------------------------------------------------------- arena props

AWRStormProp::AWRStormProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Shaft = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(RootComponent);
	Shaft->SetCollisionProfileName(TEXT("BlockAll"));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shaft->SetCanEverAffectNavigation(false);
}

void AWRStormProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStormProp, Kind);
	DOREPLIFETIME(AWRStormProp, State);
	DOREPLIFETIME(AWRStormProp, Size);
}

void AWRStormProp::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Look();
	SetState(State);
}

void AWRStormProp::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && RodHandle != INDEX_NONE)
	{
		WRStorm::RemoveRod(GetWorld(), RodHandle);
		RodHandle = INDEX_NONE;
	}
	Super::EndPlay(Reason);
}

void AWRStormProp::SetState(uint8 NewState)
{
	State = NewState;
	if (HasAuthority() && Kind == TEXT("rim_rod"))
	{
		// A standing rod's tip takes strikes (WRStorm rods); a melted or fallen one doesn't.
		if (IsStanding() && RodHandle == INDEX_NONE) RodHandle = WRStorm::AddRod(GetWorld(), this, Tip());
		else if (!IsStanding() && RodHandle != INDEX_NONE) { WRStorm::RemoveRod(GetWorld(), RodHandle); RodHandle = INDEX_NONE; }
	}
	OnRep_Look();
}

void AWRStormProp::SetCollision()
{
	// The rods, the nest and the slab are solid (they block eye bolts and bodies); melted rods are stumps.
	const bool bRod = Kind == TEXT("rim_rod");
	const float Tall = bRod ? (IsStanding() ? Size : 160.f) : 0.f;
	if (bRod)
	{
		Shaft->SetCapsuleSize(28.f, FMath::Max(30.f, Tall * 0.5f));
		Shaft->SetRelativeLocation(FVector(0.f, 0.f, Tall * 0.5f));
		Shaft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	for (UBoxComponent* B : Boxes) if (B) B->DestroyComponent();
	Boxes.Reset();
	const auto AddBox = [this](const FVector& Ext, const FVector& At, const FRotator& Rot)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetBoxExtent(Ext);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->SetupAttachment(RootComponent);
		B->RegisterComponent();
		B->SetRelativeLocationAndRotation(At, Rot);
		Boxes.Add(B);
	};
	if (Kind == TEXT("nest"))
	{
		// A ring of whole pine trunks: twelve logs round, two courses high, a gap on the arena side.
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2.f * PI * (i + 0.5f) / 12.f;
			if (FMath::Abs(FMath::FindDeltaAngleRadians(A, PI * 0.5f)) < 0.3f) continue;   // the way in (south, toward the bowl)
			const FVector P(FMath::Cos(A) * Size, FMath::Sin(A) * Size, 70.f);
			AddBox(FVector(Size * 0.28f, 45.f, 70.f), P, FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
	}
	else if (Kind == TEXT("ossuary"))
	{
		AddBox(FVector(260.f, 180.f, 25.f), FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator);
	}
	else if (Kind == TEXT("altar_rod"))
	{
		AddBox(FVector(16.f, 16.f, 160.f), FVector(-90.f, 0.f, 160.f), FRotator::ZeroRotator);
	}
}

void AWRStormProp::OnRep_Look()
{
	SetCollision();
	if (GetNetMode() == NM_DedicatedServer || (BuiltKind == Kind && BuiltState == State))
	{
		return;
	}
	Build();
}

void AWRStormProp::Build()
{
	using namespace WRLookKit;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	if (TipFx) { WRFx::Stop(TipFx); TipFx = nullptr; }
	BuiltKind = Kind;
	BuiltState = State;
	const auto Hex = [](const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); };
	const FSurf Copper = FSurf::Steel(Hex(TEXT("#B87333")), 0.4f);
	FSurf Verd = FSurf::Steel(Hex(TEXT("#4E9A84")), 0.6f);
	Verd.Metal = 0.35f;
	const FSurf Granite = FSurf::Stone(Hex(TEXT("#8A8D91")));
	const FSurf BoneSurf = FSurf::Stone(Hex(TEXT("#D8D2C4")), 0.75f);
	const FSurf Pine = FSurf::Wood(Hex(TEXT("#5A4128")));
	const FSurf Bronze = FSurf::Steel(Hex(TEXT("#8C6A3A")), 0.45f);
	TArray<FWRLookBlock> Blocks, Glow;
	FKit Kit(GetActorTransform(), Blocks);
	FKit GlowKit(GetActorTransform(), Glow);
	if (Kind == TEXT("rim_rod"))
	{
		// An old Rodwarden rod: a copper mast 8 m tall, green with age, set in a granite footing with iron bands; melted,
		// a slumped stump with a glassy fused tip; knocked down, a stump and the rest lying along the rim.
		Kit.Hex(Granite, 0.f, 0.f, -40.f, 45.f, 70.f, 15.f);
		Kit.Disc(Granite.Dim(0.8f), FVector(0.f, 0.f, 47.f), FVector::UpVector, 120.f, 6.f);
		if (IsStanding())
		{
			for (int32 s = 0; s < 6; ++s)
			{
				const float Z0 = 45.f + (Size - 45.f) * s / 6.f, Z1 = 45.f + (Size - 45.f) * (s + 1) / 6.f;
				Kit.Rod(s % 3 == 1 ? Verd : Copper, FVector(0.f, 0.f, Z0), FVector(0.f, 0.f, Z1), FMath::Lerp(22.f, 9.f, float(s) / 6.f));
				Kit.Disc(Verd.Dim(0.8f), FVector(0.f, 0.f, Z1), FVector::UpVector, FMath::Lerp(28.f, 13.f, float(s) / 6.f), 5.f);
			}
			Kit.Spike(Copper, FVector(0.f, 0.f, Size), FVector::UpVector, 10.f, 40.f);
			// The tip lights with St Elmo's fire before the rod chain (State 1).
			GlowKit.Disc(FSurf::Lit(Hex(TEXT("#B9A7FF")), State == 1 ? 6.f : 0.f), FVector(0.f, 0.f, Size + 10.f), FVector::UpVector, 20.f, 18.f);
		}
		else
		{
			const float Stub = 150.f;
			Kit.Rod(Copper.Dim(0.6f), FVector(0.f, 0.f, 45.f), FVector(8.f, 0.f, Stub), 22.f);
			Kit.Add(FSurf::Glass(Hex(TEXT("#C9D4E8")), 0.4f), FVector(40.f, 34.f, 30.f), FVector(10.f, 0.f, Stub + 8.f), FRotator(12.f, 20.f, 0.f));
			if (State == 3)
			{
				Kit.Rod(Copper, FVector(60.f, 0.f, 20.f), FVector(60.f + Size - Stub, 30.f, 20.f), 18.f);   // the fallen mast along the rim
			}
			else
			{
				Kit.Disc(FSurf::Glass(Hex(TEXT("#5E5048")), 0.f), FVector(20.f, 0.f, 4.f), FVector::UpVector, 180.f, 6.f);   // the melt puddled at its foot
			}
		}
	}
	else if (Kind == TEXT("nest"))
	{
		// Whole pine trunks laid in a ring, and hundreds of stolen copper rods twisted in like twigs.
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2.f * PI * (i + 0.5f) / 12.f;
			if (FMath::Abs(FMath::FindDeltaAngleRadians(A, PI * 0.5f)) < 0.3f) continue;
			const FVector Mid(FMath::Cos(A) * Size, FMath::Sin(A) * Size, 0.f);
			const FVector T(-FMath::Sin(A), FMath::Cos(A), 0.f);
			for (int32 c = 0; c < 2; ++c)
			{
				const float Z = 40.f + c * 75.f;
				Kit.Rod(c ? Pine.Dim(0.85f) : Pine, Mid - T * Size * 0.3f + FVector(0.f, 0.f, Z), Mid + T * Size * 0.3f + FVector(0.f, 0.f, Z + Kit.Jit(Mid, c) * 15.f), 70.f);
			}
			for (int32 r = 0; r < 14; ++r)
			{
				const FVector P = Mid + T * Kit.Jit(Mid, 10 + r) * Size * 0.28f + FVector(0.f, 0.f, 30.f + Kit.Rand(Mid, 30 + r) * 120.f);
				const FVector D = FVector(Kit.Jit(P, 1), Kit.Jit(P, 2), Kit.Jit(P, 3) * 0.5f).GetSafeNormal();
				Kit.Rod(r % 3 ? Copper : Verd, P - D * 90.f, P + D * 90.f, 5.f);
			}
		}
	}
	else if (Kind == TEXT("ossuary"))
	{
		// A flat slab covered in smashed bone, copper torcs among the bones (the lammergeier's habit).
		Kit.Add(Granite.Dim(0.9f), FVector(520.f, 360.f, 50.f), FVector(0.f, 0.f, 20.f), FRotator(0.f, 0.f, 2.f));
		for (int32 i = 0; i < 40; ++i)
		{
			const FVector P(Kit.Jit(FVector(float(i)), 1) * 230.f, Kit.Jit(FVector(float(i)), 2) * 160.f, 48.f);
			const FVector D = FRotator(0.f, Kit.Rand(P, 3) * 360.f, 0.f).Vector();
			Kit.Rod(BoneSurf.Dim(0.85f + 0.15f * Kit.Rand(P, 4)), P - D * (15.f + 25.f * Kit.Rand(P, 5)), P + D * (15.f + 25.f * Kit.Rand(P, 5)), 6.f + 6.f * Kit.Rand(P, 6));
		}
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P(Kit.Jit(FVector(float(i)), 7) * 200.f, Kit.Jit(FVector(float(i)), 8) * 140.f, 50.f);
			Kit.Ring(Copper, P, 14.f, 4.f, 4.f, 10, 0.f, 300.f, 0.6f);
		}
	}
	else if (Kind == TEXT("altar_rod"))
	{
		// The bronze rod-altar beside the slab: a bronze rod on a stepped plinth with a cup of copper at its crown.
		Kit.Hex(Granite, -90.f, 0.f, 0.f, 40.f, 60.f);
		Kit.Rod(Bronze, FVector(-90.f, 0.f, 40.f), FVector(-90.f, 0.f, 320.f), 14.f);
		Kit.Disc(Bronze.Dim(0.8f), FVector(-90.f, 0.f, 330.f), FVector::UpVector, 46.f, 10.f);
		Kit.Rod(Copper, FVector(-90.f, -40.f, 250.f), FVector(-90.f, 40.f, 250.f), 6.f);
		GlowKit.Disc(FSurf::Lit(Hex(TEXT("#B9A7FF")), 2.f), FVector(-90.f, 0.f, 337.f), FVector::UpVector, 30.f, 4.f);
	}
	else if (Kind == TEXT("veins"))
	{
		// The Copperfall's copper veins in the bowl floor: branching seams from the middle out (they light in rings).
		const FVector Mid = GetActorLocation();
		FRandomStream Rand(int32(FMath::RoundToInt(Mid.X * 0.01f) * 7919 + FMath::RoundToInt(Mid.Y * 0.01f)));
		for (int32 v = 0; v < 9; ++v)
		{
			float A = 2.f * PI * (v + Rand.FRand() * 0.6f) / 9.f;
			FVector P(FMath::Cos(A) * 300.f, FMath::Sin(A) * 300.f, 0.f);
			for (int32 s = 0; s < 10; ++s)
			{
				A += Rand.FRandRange(-0.35f, 0.35f);
				const FVector N = P + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Size / 11.f;
				bool bOk = false;
				const FVector G0 = SbGround(GetWorld(), Mid + P, bOk, 1500.f, 3000.f) - Mid;
				const FVector G1 = SbGround(GetWorld(), Mid + N, bOk, 1500.f, 3000.f) - Mid;
				Kit.Rod(s % 2 ? Verd : Copper, G0 + FVector(0.f, 0.f, 2.f), G1 + FVector(0.f, 0.f, 2.f), 22.f - s);
				GlowKit.Rod(FSurf::Lit(Hex(TEXT("#7FE0FF")), 0.f), G0 + FVector(0.f, 0.f, 6.f), G1 + FVector(0.f, 0.f, 6.f), 8.f);
				P = N;
			}
		}
	}
	const auto Spawn = [this](const TArray<FWRLookBlock>& B, bool bGlow)
	{
		if (B.Num() == 0) return;
		USceneComponent* Frame = NewObject<USceneComponent>(this);
		Frame->SetupAttachment(RootComponent);
		Frame->RegisterComponent();
		LookComps.Add(Frame);
		// The kit gave world-anchored shades; the blocks are in the actor's space.
		FWRBuildLook::Spawn(this, Frame, B, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* C : Kids) LookComps.Add(C);
		if (bGlow) GlowRoot = Frame;
	};
	Spawn(Blocks, false);
	Spawn(Glow, true);
	if (Kind == TEXT("rim_rod") && State == 1)
	{
		TipFx = WRFx::Attach(RootComponent, TEXT("storm.corona"), NAME_None, 1.f);
		if (TipFx) TipFx->SetRelativeLocation(FVector(0.f, 0.f, Size + 10.f));
		WRSound::PlayOn(this, TEXT("storm.rod_hum"));
	}
}

void AWRStormProp::MulticastPulse_Implementation(FVector_NetQuantize Centre, float Seconds)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	PulseUntil = GetWorld()->GetTimeSeconds() + Seconds;
	WRSound::PlayAt(this, TEXT("storm.ring_buzz"), Centre);
}

void AWRStormProp::MulticastArc_Implementation(FVector_NetQuantize To, float Seconds)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	ArcUntil = GetWorld()->GetTimeSeconds() + Seconds;
	ArcTo = To;
	ArcClock = 0.f;
	WRSound::PlayAt(this, TEXT("storm.arc"), (Tip() + FVector(To)) * 0.5f);
}

void AWRStormProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double T = GetWorld()->GetTimeSeconds();
	// The veins: lit while a ring runs (a flicker, then dark).
	if (GlowRoot && Kind == TEXT("veins"))
	{
		const float Glow = T < PulseUntil ? 3.f + 2.f * FMath::Sin(float(T) * 40.f) : 0.f;
		TArray<USceneComponent*> Kids;
		GlowRoot->GetChildrenComponents(false, Kids);
		for (USceneComponent* C : Kids)
		{
			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C); ISM && ISM->NumCustomDataFloats >= 10 && ISM->PerInstanceSMCustomData.IsValidIndex(8))
			{
				if (FMath::IsNearlyEqual(ISM->PerInstanceSMCustomData[8], Glow, 0.05f)) continue;
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
			}
		}
	}
	// An arc jumping from this rod's tip to the other's, low across the floor between them: drawn flickering.
	if (T < ArcUntil)
	{
		ArcClock += DeltaSeconds;
		if (ArcClock > 0.05f)
		{
			ArcClock = 0.f;
			const FVector A = GetActorLocation() + FVector(0.f, 0.f, 100.f), B = FVector(ArcTo) - FVector(0.f, 0.f, ArcTo.Z - GetActorLocation().Z - 100.f);
			const FVector D = B - A;
			WRFx::SpawnAt(this, TEXT("storm.arcline"), A, D.Rotation(), D.Size() / 100.f);
			WRFx::SpawnAt(this, TEXT("storm.arcline"), Tip(), (A - Tip()).Rotation(), (A - Tip()).Size() / 100.f);
		}
	}
}

// ---------------------------------------------------------------- data

bool FWRStormBoss::SiteAt(const UWorld* World, FName SiteId, FVector& Out)
{
	const TSharedPtr<FJsonObject> Root = SbPeaks();
	const TArray<TSharedPtr<FJsonValue>>* Sites = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("sites"), Sites))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : *Sites)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FString Id;
		if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && Id == SiteId.ToString())
		{
			Out = SbCentre(O);
			return true;
		}
	}
	(void)World;
	return false;
}

bool FWRStormBoss::NearestKit(const UWorld* World, FName Kind, const FVector& Near, float Range, FVector& Out)
{
	const TSharedPtr<FJsonObject> Root = SbPeaks();
	const TArray<TSharedPtr<FJsonValue>>* Kit = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("kit"), Kit))
	{
		return false;
	}
	float Best = Range;
	bool bFound = false;
	for (const TSharedPtr<FJsonValue>& V : *Kit)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FString K;
		if (!O.IsValid() || !O->TryGetStringField(TEXT("kind"), K) || K != Kind.ToString()) continue;
		const FVector P = SbCentre(O);
		const float D = FVector::Dist2D(P, Near);
		if (D < Best) { Best = D; Out = P; bFound = true; }
	}
	(void)World;
	return bFound;
}

// ---------------------------------------------------------------- the bosses: life

bool FWRStormBoss::IsStormBoss(const AWRBoss* Boss)
{
	return Boss && !SbKind(Boss).IsNone();
}

void FWRStormBoss::Begin(AWRBoss* Boss)
{
	if (!IsStormBoss(Boss) || !Boss->HasAuthority())
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	St.BellHP = Boss->Tune(TEXT("bellPartHealth"), 400.f);
	St.NoseHP = Boss->Tune(TEXT("nosePartHealth"), 300.f);
	St.FlightClock = SbNow(Boss);
	if (Boss->CreatureId == TEXT("galvara"))
	{
		Boss->SetAirborne(true);
		SpawnCores(Boss, FMath::RoundToInt(Boss->Tune(TEXT("cores"), 3.f)), Boss->Tune(TEXT("coreHealth"), 400.f));
	}
}

void FWRStormBoss::SpawnCores(AWRBoss* Boss, int32 Count, float Health)
{
	// Three thunderhead cores, glowing orbs 1.5 m across, orbiting 8 m under her in her banner cloud.
	FWRStormBossState& St = SbS(Boss);
	for (const TWeakObjectPtr<AWRCreature>& C : St.Cores) if (C.IsValid()) C->Destroy();
	St.Cores.Reset();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector At = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->Tune(TEXT("coreDrop"), 800.f)) + FRotator(0.f, 120.f * i, 0.f).Vector() * Boss->Tune(TEXT("coreOrbit"), 600.f);
		if (AWRCreature* Core = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("galvara_core"), At, FRotator::ZeroRotator))
		{
			Core->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			if (UAbilitySystemComponent* ASC = Core->GetAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Health);
				ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Health);
			}
			St.Cores.Add(Core);
		}
	}
	if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Set(WRStormFlag::Cloud, St.Cores.Num() > 0);
}

void FWRStormBoss::Reset(AWRBoss* Boss)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	ReleaseCarried(Boss, false);
	for (const TWeakObjectPtr<AWRCreature>& K : St.Kites) if (K.IsValid()) K->Destroy();
	St.Kites.Reset();
	St.bHerdCalled = false;
	St.BellHP = Boss->Tune(TEXT("bellPartHealth"), 400.f);
	St.NoseHP = Boss->Tune(TEXT("nosePartHealth"), 300.f);
	St.StuckUntil = St.DropUntil = St.CursedUntil = St.FrostUntil = St.BlindUntil = St.RetakeAt = 0.0;
	St.Charge = 0.f;
	St.bArmed = false;
	St.NoGainUntil = 0.0;
	St.Drops.Reset();
	St.StanceLog.Reset();
	St.Stage = 0;
	UWRStormBody* Body = UWRStormBody::Get(Boss);
	if (Body)
	{
		Body->Charge = 0;
		Body->Flags = 0;
	}
	if (Boss->CreatureId == TEXT("galvara"))
	{
		Boss->SetAirborne(true);
		SpawnCores(Boss, FMath::RoundToInt(Boss->Tune(TEXT("cores"), 3.f)), Boss->Tune(TEXT("coreHealth"), 400.f));
	}
	if (Boss->CreatureId == TEXT("coilback") && St.bUnder)
	{
		Surface(Boss, Boss->GetActorLocation());
	}
	if (Boss->CreatureId == TEXT("kharvox"))
	{
		// Rods that melted stay melted for that fight and stand again on reset.
		for (AWRStormProp* Rod : SbRods(Boss, false))
		{
			Rod->Strikes = 0;
			Rod->SetState(0);
		}
		Boss->SetAirborne(false);
	}
}

void FWRStormBoss::EndPlay(AWRBoss* Boss)
{
	if (const FWRStormBossState* St = SbFind(Boss))
	{
		for (const TWeakObjectPtr<AWRCreature>& C : St->Cores) if (C.IsValid()) C->Destroy();
		for (const TWeakObjectPtr<AWRCreature>& K : St->Kites) if (K.IsValid()) K->Destroy();
	}
	SbStates().Remove(Boss);
}

void FWRStormBoss::Died(AWRBoss* Boss)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	ReleaseCarried(Boss, false);
	FWRStormBossState& St = SbS(Boss);
	for (const TWeakObjectPtr<AWRCreature>& C : St.Cores) if (C.IsValid()) C->Destroy();
	St.Cores.Reset();
	// The herd scatters (wild Thunderhorns again).
	for (const TWeakObjectPtr<AWRCreature>& H : St.Herd) if (H.IsValid()) H->SetHome(H->GetActorLocation());
	if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Flags = 0;
}

bool FWRStormBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	bSkipWake = false;
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	if (Boss->CreatureId == TEXT("galvara"))
	{
		OrbitCores(Boss);
		return false;
	}
	if (Boss->CreatureId != TEXT("bellwether"))
	{
		return false;
	}
	const FWRCreatureDef* Def = Boss->GetDef();
	// His herd of 2 to 4 Thunderhorns walks with him.
	if (!St.bHerdMade)
	{
		St.bHerdMade = true;
		const int32 N = FMath::RandRange(FMath::RoundToInt(Boss->Tune(TEXT("herdMin"), 2.f)), FMath::RoundToInt(Boss->Tune(TEXT("herdMax"), 4.f)));
		for (int32 i = 0; i < N; ++i)
		{
			bool bOk = false;
			const FVector Spot = SbGround(Boss->GetWorld(), Boss->GetActorLocation() + FRotator(0.f, 360.f * i / N + 30.f, 0.f).Vector() * FMath::FRandRange(700.f, 1100.f), bOk);
			if (!bOk) continue;
			if (AWRCreature* Ram = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("thunderhorn"), Spot, Boss->GetActorRotation()))
			{
				St.Herd.Add(Ram);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS %s walks with %d Thunderhorns"), *Boss->GetDisplayLabel(), St.Herd.Num());
	}
	St.Herd.RemoveAll([](const TWeakObjectPtr<AWRCreature>& H) { return !H.IsValid() || !H->IsAlive() || H->IsTamed(); });
	const FVector2D Heading = FVector2D(Boss->GetVelocity()).GetSafeNormal();
	for (const TWeakObjectPtr<AWRCreature>& H : St.Herd)
	{
		if (!H->GetAggroTarget()) H->FollowPack(FVector2D(Boss->GetActorLocation()), Heading);
	}
	if (!Def || Def->Roam.Num() == 0)
	{
		return false;
	}
	// Diurnal: he sleeps at the fold at night (a sneak's crit opener), and rests three minutes there by day.
	const FVector Fold = Def->Roam[0];
	const bool bAtFold = FVector::Dist2D(Boss->GetActorLocation(), Fold) < Boss->Tune(TEXT("foldRadius"), 500.f);
	if (AWRGameState::IsNightIn(Boss))
	{
		if (!bAtFold)
		{
			Boss->SetAsleep(false);
			Boss->SetHome(FVector(Fold.X, Fold.Y, Boss->GetActorLocation().Z));
			Boss->AddMovementInput((Fold - Boss->GetActorLocation()).GetSafeNormal2D(), WRTune(TEXT("bosses.roamInput"), 0.45f));
			return true;
		}
		Boss->SetAsleep(true);
		for (const TWeakObjectPtr<AWRCreature>& H : St.Herd) H->SetAsleep(true);
		// Asleep he notices you late (creatureAI.sleepAggroMult of his range).
		bSkipWake = true;
		for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) < Def->AggroRadius * WRTune(TEXT("creatureAI.sleepAggroMult"), 0.35f)) bSkipWake = false;
		}
		Boss->RoamIndex = 0;
		return true;
	}
	Boss->SetAsleep(false);
	if (bAtFold && T - St.LastFold > Boss->Tune(TEXT("foldRestSeconds"), 180.f) * 2.0)
	{
		St.LastFold = T;
		St.RestUntil = T + Boss->Tune(TEXT("foldRestSeconds"), 180.f);
	}
	return T < St.RestUntil;
}

void FWRStormBoss::OrbitCores(AWRBoss* Boss)
{
	FWRStormBossState& St = SbS(Boss);
	const float T = SbNow(Boss);
	const int32 N = St.Cores.Num();
	for (int32 i = 0; i < N; ++i)
	{
		AWRCreature* Core = St.Cores[i].Get();
		if (!Core || !Core->IsAlive()) continue;
		const float A = T * Boss->Tune(TEXT("coreSpin"), 0.4f) + 2.f * PI * i / FMath::Max(1, N);
		const FVector At = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->Tune(TEXT("coreDrop"), 800.f)) + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Boss->Tune(TEXT("coreOrbit"), 600.f);
		Core->SetActorLocation(FMath::VInterpTo(Core->GetActorLocation(), At, Boss->GetWorld()->GetDeltaSeconds(), 3.f));
	}
}

bool FWRStormBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	FWRStormBossState& St = SbS(Boss);
	UWRStormBody* Body = UWRStormBody::Get(Boss);
	const double T = SbNow(Boss);
	const FName Id = Boss->CreatureId;

	// A grabbed body's fall ends on the slab (the landing is the hurt; a full-health survivor lives through it).
	for (int32 i = St.Drops.Num() - 1; i >= 0; --i)
	{
		FWRStormBossDrop& D = St.Drops[i];
		AWRCharacterBase* Who = D.Who.Get();
		if (!Who || !Who->IsAlive() || T - D.Since > 8.0) { St.Drops.RemoveAt(i); continue; }
		if (T - D.Since > 0.3 && !Who->GetCharacterMovement()->IsFalling())
		{
			const float Fall = FMath::Max(0.f, D.FromZ - float(Who->GetActorLocation().Z)) / 100.f;
			const UWRAttributeSet* A = Who->GetAttributes();
			const float Dmg = FMath::Min(Fall * Boss->Tune(TEXT("boneDropPerMetre"), 4.5f), (A ? A->GetMaxHealth() : 100.f) * Boss->Tune(TEXT("boneDropMaxShare"), 0.6f));
			if (Dmg > 1.f)
			{
				FWRHit Hit;
				Hit.Instigator = Boss;
				Hit.Damage = Dmg;
				Hit.bUnblockable = true;
				Hit.Impact = 5;
				Hit.Tier = EWRHitTier::Heavy;
				Hit.Origin = Who->GetActorLocation() + FVector(0.f, 0.f, 100.f);
				Hit.Element = WRTags::Element_Physical;
				UWRCombatLibrary::ApplyHit(Who, Hit);
				WRFx::Broadcast(Boss, TEXT("land.dust"), SbFeet(Who), FRotator::ZeroRotator, 1.6f, TEXT("land"));
			}
			St.Drops.RemoveAt(i);
		}
	}

	// The opposite element's counter, seen as it lands: a bleed proc, a curse proc, a frost proc.
	const bool bBleed = Boss->HasTag(WRTags::Status_Bleeding);
	const bool bCurse = Boss->HasTag(WRTags::Status_Curse);
	const bool bRime = Boss->HasTag(WRTags::Status_Rimed) || Boss->HasTag(WRTags::Status_Frostbite);
	if (bBleed && !St.bBleed)
	{
		if (Id == TEXT("bellwether"))
		{
			// Bleed cuts his charge: each proc adds 8 s to its cooldown.
			St.ChargeReadyAt = FMath::Max(St.ChargeReadyAt, T) + Boss->Tune(TEXT("bleedChargeAdd"), 8.f);
		}
		else if (Id == TEXT("kharvox"))
		{
			St.Charge = FMath::Max(0.f, St.Charge - Boss->Tune(TEXT("bleedChargeLoss"), 30.f));
			St.NoGainUntil = T + Boss->Tune(TEXT("bleedChargeLock"), 10.f);
			St.bArmed = false;
		}
	}
	if (bCurse && !St.bCurse && Id == TEXT("kharvox"))
	{
		// A curse grounds him: he crashes down, wings dragging, and takes x1.25 meanwhile.
		St.CursedUntil = T + Boss->Tune(TEXT("curseGroundSeconds"), 8.f);
		Boss->GroundedUntil = St.CursedUntil;
		if (Boss->bAirborne) Boss->SetAirborne(false);
		Boss->Status->Stagger(Boss->Tune(TEXT("curseCrashStagger"), 1.5f));
		Boss->Current = INDEX_NONE;
		Boss->StrikeState = AWRBoss::EStrike::None;
		WRFx::Broadcast(Boss, TEXT("stomp"), SbFeet(Boss), FRotator::ZeroRotator, 2.5f, TEXT("stomp"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS Kharvox cursed: grounded"));
	}
	if (bRime && !St.bRime && Id == TEXT("coilback"))
	{
		// Frost slows his tunnelling (x0.4 for 12 s) and leaves a rime line over his path.
		St.FrostUntil = T + Boss->Tune(TEXT("frostTunnelSeconds"), 12.f);
	}
	St.bBleed = bBleed;
	St.bCurse = bCurse;
	St.bRime = bRime;

	if (Id == TEXT("bellwether"))
	{
		if (Body) Body->Set(WRStormFlag::Stuck, T < St.StuckUntil);
		// His herd joins any fight within 30 m.
		AWRCharacterBase* Foe = Boss->AttackTarget.IsValid() ? Boss->AttackTarget.Get() : Boss->PickBossTarget();
		St.Herd.RemoveAll([](const TWeakObjectPtr<AWRCreature>& H) { return !H.IsValid() || !H->IsAlive() || H->IsTamed(); });
		for (const TWeakObjectPtr<AWRCreature>& H : St.Herd)
		{
			if (Foe && !H->GetAggroTarget() && FVector::Dist(H->GetActorLocation(), Boss->GetActorLocation()) < Boss->Tune(TEXT("herdJoinRadius"), 3000.f))
			{
				H->SetAsleep(false);
				H->SetAggroTarget(Foe);
			}
		}
		Boss->SetAsleep(false);
		return false;
	}
	if (Id == TEXT("galvara"))
	{
		return TickGalvara(Boss, St, Body, DeltaSeconds);
	}
	if (Id == TEXT("coilback"))
	{
		return TickCoilback(Boss, St, Body, DeltaSeconds);
	}
	return TickKharvox(Boss, St, Body, DeltaSeconds);
}

bool FWRStormBoss::TickGalvara(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds)
{
	const double T = SbNow(Boss);
	OrbitCores(Boss);
	St.Cores.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
	St.Kites.RemoveAll([](const TWeakObjectPtr<AWRCreature>& K) { return !K.IsValid() || !K->IsAlive(); });
	if (Body) Body->Set(WRStormFlag::Cloud, Boss->bAirborne && St.Cores.Num() > 0);
	if (Boss->bAirborne && St.Cores.Num() == 0 && St.RetakeAt <= 0.0)
	{
		// The last core broke: the cloud tears, she drops to the crag: 6 s stunned, x1.5 damage taken.
		Boss->SetAirborne(false);
		Boss->Current = INDEX_NONE;
		Boss->StrikeState = AWRBoss::EStrike::None;
		St.DropUntil = T + Boss->Tune(TEXT("dropStun"), 6.f);
		St.GroundSince = T;
		St.StanceLog.Reset();
		UWRCombatLibrary::BreakStance(Boss, Boss->Tune(TEXT("dropStun"), 6.f), nullptr);
		WRSound::Voice(Boss, TEXT("hurt"));
		WRFx::Broadcast(Boss, TEXT("storm.clap"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("storm.cloud_static"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS Galvara's cloud tears: she drops"));
		return true;
	}
	if (Boss->bAirborne)
	{
		// The rain of strikes: one per living core, every 6 s, on the spots of up to 3 survivors (the same one 0.4 s apart).
		if (T >= St.NextRainAt && St.Cores.Num() > 0)
		{
			St.NextRainAt = T + Boss->Tune(TEXT("strikeEvery"), 6.f);
			TArray<AWRCharacterBase*> Who = SbFoes(Boss, Boss->ChaseRadius, true);
			Who.Sort([Boss](const AWRCharacterBase& A, const AWRCharacterBase& B) { return FVector::DistSquared(A.GetActorLocation(), Boss->GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), Boss->GetActorLocation()); });
			if (Who.Num() > 3) Who.SetNum(3);
			for (int32 i = 0; i < St.Cores.Num() && Who.Num() > 0; ++i)
			{
				TWeakObjectPtr<AWRCharacterBase> Victim(Who[i % Who.Num()]);
				TWeakObjectPtr<AWRBoss> WeakBoss(Boss);
				const float Delay = 0.01f + 0.4f * (i / Who.Num());
				FTimerHandle H;
				Boss->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Boss, [WeakBoss, Victim]()
				{
					AWRBoss* B = WeakBoss.Get();
					AWRCharacterBase* V = Victim.Get();
					if (!B || !V || !V->IsAlive() || !B->bAirborne) return;
					FWRStrikeCall Call;
					Call.Spot = SbFeet(V);
					Call.Tell = B->Tune(TEXT("strikeTell"), 1.f);
					Call.Damage = B->Tune(TEXT("strikeDamage"), 45.f) * SbScale();
					Call.Shock = B->Tune(TEXT("strikeShock"), 40.f);
					Call.Source = EWRStrikeSource::Creature;
					Call.Instigator = B;
					WRStorm::CallStrike(B, Call);
				}), Delay, false);
			}
			WRSound::PlayOn(Boss, TEXT("storm.cloud_static"));
		}
		return false;
	}
	// On the crag: if she takes less than 150 stance damage in any 20 s, she leaps back up and reforms her cloud with
	// one core (500 HP).
	if (T < St.DropUntil)
	{
		return false;
	}
	if (St.RetakeAt > 0.0)
	{
		if (T >= St.RetakeAt)
		{
			St.RetakeAt = 0.0;
			Boss->SetAirborne(true);
			Boss->LaunchCharacter(FVector(0.f, 0.f, 1600.f), true, true);
			SpawnCores(Boss, 1, Boss->Tune(TEXT("reformCoreHealth"), 500.f));
			St.NextRainAt = T + 3.0;
			UE_LOG(LogWildreach, Log, TEXT("BOSS Galvara retakes the sky"));
		}
		return true;
	}
	if (St.GroundSince <= 0.0) St.GroundSince = T;
	const float Window = Boss->Tune(TEXT("retakeWindow"), 20.f);
	St.StanceLog.RemoveAll([T, Window](const TPair<double, float>& E) { return T - E.Key > Window; });
	float Taken = 0.f;
	for (const TPair<double, float>& E : St.StanceLog) Taken += E.Value;
	if (T - St.GroundSince > Window && Taken < Boss->Tune(TEXT("retakeStance"), 150.f) && Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered))
	{
		St.RetakeAt = T + Boss->Tune(TEXT("retakeWindup"), 1.2f);
		St.GroundSince = 0.0;
		Boss->PlaySwing(EWRSwingKind::Heavy, Boss->Tune(TEXT("retakeWindup"), 1.2f));
		Boss->MulticastTell(1, TEXT("wing"), TEXT("voice"));
		return true;
	}
	(void)DeltaSeconds;
	return false;
}

void FWRStormBoss::Surface(AWRBoss* Boss, const FVector& At)
{
	FWRStormBossState& St = SbS(Boss);
	St.bUnder = false;
	bool bOk = false;
	const FVector G = SbGround(Boss->GetWorld(), At, bOk);
	Boss->SetActorLocation(G + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 10.f), false, nullptr, ETeleportType::TeleportPhysics);
	Boss->SetActorHiddenInGame(false);
	Boss->SetActorEnableCollision(true);
	Boss->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (UWRStormBody* Body = UWRStormBody::Get(Boss))
	{
		Body->Set(WRStormFlag::Underground, false);
		Body->Set(WRStormFlag::Rimed, false);
	}
}

bool FWRStormBoss::TickCoilback(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds)
{
	const double T = SbNow(Boss);
	const bool bBlind = T < St.BlindUntil;
	if (Body)
	{
		Body->Set(WRStormFlag::Blind, bBlind);
		Body->Set(WRStormFlag::Sensing, !bBlind && Boss->bEngaged);
		Body->Set(WRStormFlag::Rimed, St.bUnder && T < St.FrostUntil);
	}
	if (!St.bUnder)
	{
		return false;
	}
	// Under the scree: a ridge of soil moves over his path toward the one wearing the most metal (blind: anywhere near).
	AWRCharacterBase* Target = PickTarget(Boss, Boss->PickBossTarget());
	FVector Goal = Target ? Target->GetActorLocation() : Boss->GetHome();
	if (bBlind)
	{
		if (St.WanderTo.IsZero() || FVector::Dist2D(St.WanderTo, Boss->GetActorLocation()) < 200.f) St.WanderTo = Boss->GetHome() + FVector(FMath::RandPointInCircle(Boss->ArenaRadius * 0.8f), 0.f);
		Goal = St.WanderTo;
	}
	const FWRCreatureDef* Def = Boss->GetDef();
	const float Speed = (Def ? Def->Speed : 400.f) * Boss->Tune(TEXT("tunnelSpeedMult"), 1.1f) * (T < St.FrostUntil ? Boss->Tune(TEXT("frostTunnelMult"), 0.4f) : 1.f);
	if (Boss->Current == INDEX_NONE || (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id != TEXT("surface_burst")))
	{
		const FVector To = (Goal - Boss->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (To.Size2D() > 150.f)
		{
			const FVector Next = Boss->GetActorLocation() + To.GetSafeNormal() * FMath::Min(To.Size2D(), Speed * DeltaSeconds);
			bool bOk = false;
			const FVector G = SbGround(Boss->GetWorld(), Next, bOk, 2000.f, 4000.f);
			Boss->SetActorLocation(FVector(Next.X, Next.Y, bOk ? G.Z + Boss->GetSimpleCollisionHalfHeight() : Next.Z), false);
			Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}
	// Under too long: he comes up under his target.
	if (T > St.SurfaceBy && Boss->Current == INDEX_NONE && Target)
	{
		const int32 Burst = Boss->Attacks.IndexOfByPredicate([](const FWRBossAttack& A) { return A.Id == TEXT("surface_burst"); });
		if (Boss->Attacks.IsValidIndex(Burst))
		{
			Boss->StartAttack(Boss->Attacks[Burst], Target);
			return true;
		}
		Surface(Boss, Boss->GetActorLocation());
	}
	return false;
}

void FWRStormBoss::ReleaseCarried(AWRBoss* Boss, bool bDrop)
{
	FWRStormBossState& St = SbS(Boss);
	AWRCharacterBase* Who = St.Carried.Get();
	St.Carried = nullptr;
	if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Set(WRStormFlag::Grounded, false);
	if (!Who)
	{
		return;
	}
	Who->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Who->SetActorEnableCollision(true);
	Who->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Who->Status->ClearStatus(WRTags::Status_Staggered);
	if (bDrop)
	{
		FWRStormBossDrop D;
		D.Who = Who;
		D.Since = SbNow(Boss);
		D.FromZ = Who->GetActorLocation().Z;
		St.Drops.Add(D);
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS Kharvox lets %s go (%s)"), *Who->GetName(), bDrop ? TEXT("dropped") : TEXT("freed"));
}

bool FWRStormBoss::TickKharvox(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds)
{
	const double T = SbNow(Boss);
	// His Charge: +6 a second airborne (none while bleeding), shown on his feathers.
	const bool bGain = T >= St.NoGainUntil;
	if (Boss->bAirborne && bGain) St.Charge += Boss->Tune(TEXT("chargeAir"), 6.f) * DeltaSeconds;
	St.Charge = FMath::Clamp(St.Charge, 0.f, Boss->Tune(TEXT("chargeMax"), 100.f));
	if (St.Charge >= Boss->Tune(TEXT("chargeMax"), 100.f) && !St.bArmed)
	{
		St.bArmed = true;
		WRSound::PlayOn(Boss, TEXT("storm.charge_whine"));
	}
	if (Body)
	{
		Body->Charge = uint8(FMath::RoundToInt(St.Charge));
		Body->Set(WRStormFlag::Dimmed, !bGain);
		Body->Set(WRStormFlag::Grounded, T < St.CursedUntil);
	}

	// The bone drop: carrying its catch up and over the ossuary slab, then letting go.
	if (AWRCharacterBase* Who = St.Carried.Get())
	{
		if (!Who->IsAlive())
		{
			ReleaseCarried(Boss, false);
			return true;
		}
		FVector Slab = Boss->GetHome();
		SbMetres(SbArena(TEXT("kharvox")), TEXT("ossuary"), Slab);
		bool bOk = false;
		const FVector SlabG = SbGround(Boss->GetWorld(), FVector(Slab.X, Slab.Y, Boss->GetHome().Z), bOk, 5000.f, 8000.f);
		const FVector Goal = SlabG + FVector(0.f, 0.f, St.CarryUp + Boss->GetSimpleCollisionHalfHeight());
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Goal, DeltaSeconds, Boss->Tune(TEXT("boneDropClimbSpeed"), 900.f)), false);
		Boss->SetActorRotation(FRotator(0.f, (Goal - Boss->GetActorLocation()).Rotation().Yaw, 0.f));
		if (FVector::Dist(Boss->GetActorLocation(), Goal) < 120.f || T - St.CarryStart > Boss->Tune(TEXT("boneDropMaxCarry"), 4.f))
		{
			ReleaseCarried(Boss, true);
			Boss->BusyUntil = T + 1.0;
		}
		return true;
	}

	// Flight: in the air a while, then down a while (phase 3: the air only, down only to grab).
	if (Boss->Phase >= 3)
	{
		if (!Boss->bAirborne && T >= Boss->GroundedUntil && T >= St.CursedUntil && Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered)) { Boss->SetAirborne(true); St.FlightClock = T; }
	}
	else if (Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered))
	{
		if (Boss->bAirborne && T - St.FlightClock > Boss->Tune(TEXT("airSeconds"), 10.f))
		{
			Boss->SetAirborne(false);
			St.FlightClock = T;
		}
		else if (!Boss->bAirborne && T - St.FlightClock > Boss->Tune(TEXT("landedSeconds"), 12.f) && T >= Boss->GroundedUntil && T >= St.CursedUntil)
		{
			Boss->SetAirborne(true);
			St.FlightClock = T;
		}
	}

	// The eye of the storm (phase 3): a 10 m calm drifting round the arena at 1 m/s; no strike lands in it.
	if (Boss->Phase >= 3)
	{
		if (St.EyeAt.IsZero()) St.EyeAt = Boss->GetHome();
		if (T >= St.EyeTurnAt)
		{
			St.EyeTurnAt = T + FMath::FRandRange(6.f, 10.f);
			St.EyeDir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
		}
		const float Keep = Boss->ArenaRadius * 0.55f;
		FVector Next = St.EyeAt + St.EyeDir * Boss->Tune(TEXT("eyeSpeed"), 100.f) * DeltaSeconds;
		if (FVector::Dist2D(Next, Boss->GetHome()) > Keep)
		{
			St.EyeDir = (Boss->GetHome() - St.EyeAt).GetSafeNormal2D().RotateAngleAxis(FMath::FRandRange(-40.f, 40.f), FVector::UpVector);
			Next = St.EyeAt;
		}
		St.EyeAt = Next;
		bool bOk = false;
		if (Body)
		{
			Body->EyeAt = SbGround(Boss->GetWorld(), St.EyeAt, bOk, 4000.f, 6000.f);
			Body->Set(WRStormFlag::Eye, true);
		}
	}

	// Called strikes (phase 2 on): a strike on each survivor's spot in turn, every 4 s (2 s in the eye of the storm).
	// Rod shadow: one within 3 m of a standing rod's foot is never picked; the rod takes it (and melts after 6).
	if (Boss->Phase >= 2 && Boss->PendingPhase == 0 && T >= St.NextCalledAt)
	{
		St.NextCalledAt = T + Boss->Tune(Boss->Phase >= 3 ? TEXT("p3CalledStrikeEvery") : TEXT("calledStrikeEvery"), Boss->Phase >= 3 ? 2.f : 4.f);
		TArray<AWRCharacterBase*> Who = SbFoes(Boss, Boss->ChaseRadius, true);
		Who.Sort([](const AWRCharacterBase& A, const AWRCharacterBase& B) { return A.GetName() < B.GetName(); });
		const TArray<AWRStormProp*> Standing = SbRods(Boss, true);
		for (int32 Try = 0; Try < Who.Num(); ++Try)
		{
			AWRCharacterBase* V = Who[(St.CalledTurn + Try) % Who.Num()];
			if (Boss->Phase >= 3 && FVector::Dist2D(V->GetActorLocation(), St.EyeAt) < Boss->Tune(TEXT("eyeRadius"), 1000.f)) continue;   // in the calm
			St.CalledTurn = (St.CalledTurn + Try + 1) % FMath::Max(1, Who.Num());
			AWRStormProp* Shade = nullptr;
			for (AWRStormProp* Rod : Standing)
			{
				if (FVector::Dist2D(Rod->GetActorLocation(), V->GetActorLocation()) < Boss->Tune(TEXT("rodShadow"), 300.f)) { Shade = Rod; break; }
			}
			FWRStrikeCall Call;
			Call.PreTell = Shade ? 0.f : Boss->Tune(TEXT("calledPreTell"), 3.f);
			Call.Tell = Boss->Tune(TEXT("calledTell"), 1.f);
			Call.Damage = Boss->Tune(TEXT("calledDamage"), 50.f) * SbScale() * (Boss->Phase >= 3 ? Boss->Tune(TEXT("phase3DamageScale"), 1.4f) : 1.f);
			Call.Shock = Boss->Tune(TEXT("calledShock"), 50.f);
			Call.Source = EWRStrikeSource::Creature;
			Call.Instigator = Boss;
			Call.Picked = V;
			if (Shade)
			{
				Call.Spot = Shade->GetActorLocation();
				Call.bRodsTake = true;
				if (++Shade->Strikes >= FMath::RoundToInt(Boss->Tune(TEXT("rodStrikesToMelt"), 6.f)))
				{
					TWeakObjectPtr<AWRStormProp> WeakRod(Shade);
					FTimerHandle H;
					Boss->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Shade, [WeakRod]() { if (WeakRod.IsValid()) WeakRod->SetState(2); }), Call.Tell + 0.2f, false);
				}
			}
			else
			{
				Call.Spot = SbFeet(V);
			}
			WRStorm::CallStrike(Boss, Call);
			break;
		}
	}
	return false;
}

AWRCharacterBase* FWRStormBoss::PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest)
{
	if (!Boss || Boss->CreatureId != TEXT("coilback"))
	{
		return Nearest;
	}
	const FWRStormBossState* St = SbFind(Boss);
	if (St && SbNow(Boss) < St->BlindUntil)
	{
		return Nearest;   // the star nose broken: no metal sense
	}
	// He goes for whoever wears the most metal (heavy, then medium); take your metal off and he picks someone else.
	AWRCharacterBase* Best = Nearest;
	float BestKg = 0.f;
	for (AWRCharacterBase* Who : SbFoes(Boss, Boss->ChaseRadius))
	{
		float Kg = 0.f;
		if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who))
		{
			for (const EWREquipSlot Slot : WRGearSlots::All)
			{
				const FWRItemStack& Stack = Inv->GetEquipped(Slot);
				const FWRItemDef* Def = Stack.ItemId.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Stack.ItemId);
				if (Def && !Def->Metal.IsNone() && Def->Metal != TEXT("wood") && Def->Metal != TEXT("stone") && Def->Metal != TEXT("hide") && Def->Metal != TEXT("leather") && Def->Metal != TEXT("bone")) Kg += FMath::Max(0.5f, Def->Weight);
			}
		}
		if (Kg > BestKg + 0.01f) { BestKg = Kg; Best = Who; }
	}
	return Best;
}

// ---------------------------------------------------------------- the attack machine

bool FWRStormBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsStormBoss(Boss))
	{
		return true;
	}
	const FWRStormBossState* St = SbFind(Boss);
	const double T = SbNow(Boss);
	const FName Id = Attack.Id;
	const float Dist = Target ? FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) : 0.f;
	if (Boss->CreatureId == TEXT("bellwether"))
	{
		if (Id == TEXT("ridge_charge")) return (!St || T >= St->ChargeReadyAt) && !Boss->HasTag(WRTags::Status_Bleeding);
		if (Id == TEXT("bell_toll")) return !Boss->IsPartBroken(TEXT("bell"));
	}
	else if (Boss->CreatureId == TEXT("galvara"))
	{
		if (Id == TEXT("kite_call"))
		{
			int32 Alive = 0;
			if (St) for (const TWeakObjectPtr<AWRCreature>& K : St->Kites) Alive += K.IsValid() && K->IsAlive();
			return Alive + 2 <= FMath::RoundToInt(WRTune(TEXT("bosses.galvara.maxKites"), 4.f));
		}
		if (St && St->RetakeAt > 0.0) return false;
	}
	else if (Boss->CreatureId == TEXT("coilback"))
	{
		const bool bUnder = St && St->bUnder;
		const bool bUnderMove = Id == TEXT("ring_pulse") || Id == TEXT("surface_burst");
		if (bUnder != bUnderMove) return false;
		if (Id == TEXT("scree_spray") && Dist < WRTune(TEXT("creatureAI.storm.minRanged"), 400.f)) return false;
	}
	else if (Boss->CreatureId == TEXT("kharvox"))
	{
		if (St && St->Carried.IsValid()) return false;
		// Phase 3: the air only (down only to grab).
		if (Boss->Phase >= 3 && Attack.Height == 0 && Id != TEXT("bone_drop")) return false;
		if ((Id == TEXT("eye_bolt") || Id == TEXT("double_eye_bolt")) && Dist < WRTune(TEXT("creatureAI.storm.minRanged"), 400.f)) return false;
		if (Id == TEXT("eye_bolt") && Boss->Phase >= 2 && FMath::FRand() < 0.5f) return false;   // phase 2 mixes in the double
		if (Id == TEXT("rod_chain")) return SbRods(Boss, true).Num() >= 2;
		if (Id == TEXT("bone_drop") && Boss->bAirborne && Boss->Phase < 3) return false;
	}
	return true;
}

void FWRStormBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	const FName Id = Attack.Id;
	St.Stage = 0;
	St.StageAt = T;
	St.bImpacted = false;
	St.bSecondFired = false;
	St.Aim = Boss->AttackPoint;
	if (Boss->CreatureId == TEXT("bellwether") && Id == TEXT("ridge_charge"))
	{
		St.ChargeReadyAt = T + Boss->Tune(TEXT("chargeCooldown"), 10.f);
		WRSound::PlayOn(Boss, TEXT("storm.bell_clank"));
	}
	else if (Boss->CreatureId == TEXT("galvara") && Id == TEXT("tail_lash_pass") && Target)
	{
		// She drops out of the cloud along a straight line across the top at 3 m; the line shows as a swirl in the
		// cloud 0.9 s before.
		const FVector Dir = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		const FVector Spot = SbFeet(Target);
		const float Half = Boss->Tune(TEXT("lashHalfLength"), 1500.f);
		St.LashFrom = Spot - Dir * Half + FVector(0.f, 0.f, 300.f + Boss->GetSimpleCollisionHalfHeight());
		St.LashTo = Spot + Dir * Half + FVector(0.f, 0.f, 300.f + Boss->GetSimpleCollisionHalfHeight());
		for (int32 i = 0; i <= 4; ++i)
		{
			const FVector P = FMath::Lerp(St.LashFrom, St.LashTo, i / 4.f);
			WRFx::Broadcast(Boss, TEXT("storm.swirl"), FVector(P.X, P.Y, Boss->GetActorLocation().Z - Boss->Tune(TEXT("coreDrop"), 800.f)), Dir.Rotation(), 2.f, i == 2 ? FName(TEXT("storm.cloud_static")) : NAME_None, Attack.Windup);
		}
	}
	else if (Boss->CreatureId == TEXT("coilback"))
	{
		if (Id == TEXT("ring_pulse"))
		{
			// The copper veins light in rings round his hidden spot, with a buzz under your feet.
			for (TActorIterator<AWRStormProp> It(Boss->GetWorld()); It; ++It)
			{
				if (It->Kind == TEXT("veins") && FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) < 6000.f) It->MulticastPulse(Boss->GetActorLocation(), Attack.Windup + 2.5f);
			}
			bool bOk = false;
			St.RingAt = SbGround(Boss->GetWorld(), Boss->GetActorLocation(), bOk);
		}
		else if (Id == TEXT("surface_burst"))
		{
			// The ground bulges and scree hops in a 6 m circle under his target; blind, he comes up near, not on.
			const bool bBlind = T < St.BlindUntil;
			FVector Spot = Target ? SbFeet(Target) : Boss->GetActorLocation();
			if (bBlind) Spot += FVector(FMath::RandPointInCircle(Boss->Tune(TEXT("blindMiss"), 600.f)), 0.f);
			Boss->AttackPoint = Spot;
			if (bBlind)
			{
				Boss->ResolveAt = Boss->StrikeStarted + Boss->Tune(TEXT("blindBurstWindup"), 1.2f);
			}
			WRFx::Broadcast(Boss, TEXT("burst.tell"), Spot, FRotator::ZeroRotator, Attack.Size / 150.f, TEXT("storm.burrow"), float(Boss->ResolveAt - T));
		}
	}
	else if (Boss->CreatureId == TEXT("kharvox"))
	{
		if (Id == TEXT("rod_chain"))
		{
			// Three random pairs of standing rods light at the tips (St Elmo's fire), with a buzz.
			TArray<AWRStormProp*> Standing = SbRods(Boss, true);
			St.Pairs.Reset();
			for (int32 i = Standing.Num() - 1; i > 0; --i) Standing.Swap(i, FMath::RandRange(0, i));
			for (int32 p = 0; p + 1 < Standing.Num() && St.Pairs.Num() < FMath::RoundToInt(Boss->Tune(TEXT("rodChainPairs"), 3.f)); p += 2)
			{
				St.Pairs.Add({ Standing[p], Standing[p + 1] });
				Standing[p]->SetState(1);
				Standing[p + 1]->SetState(1);
			}
		}
		else if (Id == TEXT("thunderclap"))
		{
			Boss->SetAirborne(true);
			St.Aim = Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->Tune(TEXT("clapRise"), 1000.f));
		}
		else if (Id == TEXT("storm_pillar"))
		{
			Boss->SetAirborne(true);
			St.Aim = Target ? SbFeet(Target) : Boss->GetActorLocation();
		}
		else if (Id == TEXT("talon_dive") || Id == TEXT("sky_dive"))
		{
			St.Stage = Boss->bAirborne && Id == TEXT("sky_dive") ? 1 : 0;
			St.CarryFrom = Boss->GetActorLocation();
			Boss->SetAirborne(true);
			if (St.Stage == 1) St.StageAt = T - Boss->Tune(TEXT("diveClimb"), 1.2f);
		}
		else if (Id == TEXT("eye_bolt") || Id == TEXT("double_eye_bolt"))
		{
			WRSound::PlayOn(Boss, TEXT("storm.charge_whine"));
		}
	}
}

bool FWRStormBoss::TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	const FName Id = Attack.Id;
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	if (Id == TEXT("tail_lash_pass"))
	{
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), St.LashFrom, DeltaSeconds, Boss->Tune(TEXT("lashApproachSpeed"), 5000.f)), false);
		Boss->SetActorRotation(FRotator(0.f, (St.LashTo - St.LashFrom).Rotation().Yaw, 0.f));
	}
	else if (Id == TEXT("talon_dive") || Id == TEXT("sky_dive"))
	{
		const float Climb = Boss->Tune(TEXT("diveClimb"), 1.2f);
		if (St.Stage == 0)
		{
			// Climbs 40 m.
			const float A = FMath::Clamp(float(T - St.StageAt) / Climb, 0.f, 1.f);
			Boss->SetActorLocation(St.CarryFrom + FVector(0.f, 0.f, Boss->Tune(TEXT("diveHeight"), 4000.f) * FMath::Sin(A * PI * 0.5f)), false);
			if (A >= 1.f) St.Stage = 1;
		}
		if (St.Stage == 1)
		{
			// The fold: the aim locks here (crimson at the fold), his shadow grows on the floor.
			St.Stage = 2;
			St.Aim = Target ? SbFeet(Target) : Boss->AttackPoint;
			Boss->AttackPoint = St.Aim;
			Boss->MulticastTell(2, TEXT("Foot"), TEXT("tell.final"));
			const float Left = FMath::Max(0.2f, float(Boss->ResolveAt - T));
			// His own shadow (he's folded high over the spot): his body's size, not the strike's whole reach (3.11).
			if (AWRGroundMarker* Shade = AWRGroundMarker::Spawn(Boss->GetWorld(), St.Aim + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector2D(FMath::Min(Attack.Size, Boss->GetSimpleCollisionRadius() * 1.6f)), false, FLinearColor(0.02f, 0.02f, 0.025f), Left + 0.4f))
			{
				St.Shadow = Shade;
			}
		}
		if (St.Stage == 2)
		{
			Boss->SetActorRotation(FRotator(0.f, (St.Aim - Boss->GetActorLocation()).Rotation().Yaw, 0.f));
		}
	}
	else if (Id == TEXT("eye_bolt") || Id == TEXT("double_eye_bolt"))
	{
		// The aim locks on the spot where the target was; a faint stepped leader crawls along the line for
		// the last boltLead s, then the stroke.
		// (cookpass-combat 3.12: the leader shows boltLead s, at least the 0.9 s every shot gives, so the lock comes that
		// long before the stroke.)
		if (St.Stage == 0 && T - Boss->StrikeStarted >= FMath::Max(0.f, float(Boss->ResolveAt - Boss->StrikeStarted) - Boss->Tune(TEXT("boltLead"), 0.9f)))
		{
			St.Stage = 1;
			St.Aim = Target ? Target->GetActorLocation() : Boss->AttackPoint;
			SbLine(Boss, TEXT("storm.boltleader"), EyePoint(Boss), BoltEnd(Boss, EyePoint(Boss), St.Aim), TEXT("storm.spark"));
		}
	}
	else if (Id == TEXT("storm_pillar"))
	{
		const FVector Over = St.Aim + FVector(0.f, 0.f, Boss->Tune(TEXT("pillarHeight"), 1200.f) + Boss->GetSimpleCollisionHalfHeight());
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Over, DeltaSeconds, Boss->Tune(TEXT("pillarMoveSpeed"), 2500.f)), false);
	}
	else if (Id == TEXT("thunderclap"))
	{
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), St.Aim, DeltaSeconds, 900.f), false);
	}
	return false;
}

FVector FWRStormBoss::EyePoint(const AWRBoss* Boss)
{
	return Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 0.9f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.5f);
}

FVector FWRStormBoss::BoltEnd(const AWRBoss* Boss, const FVector& From, const FVector& Aim)
{
	// Range 30 m, blocked by the nest and the rods (solid world), never homing.
	const FVector Dir = (Aim - From).GetSafeNormal();
	const FVector To = From + Dir * FMath::Min(float(FVector::Dist(From, Aim)) + 300.f, Boss->Tune(TEXT("boltRange"), 3000.f));
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRStormBolt), false, Boss);
	return Boss->GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q) ? Hit.ImpactPoint : To;
}

void FWRStormBoss::Stroke(AWRBoss* Boss, const FVector& From, const FVector& To)
{
	// The bright stroke: everything on the line takes it (not within 4 m of where it left him).
	SbLine(Boss, TEXT("storm.bolt"), From, To, TEXT("storm.strike"));
	for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
	{
		FVector OnLine, OnBody;
		FMath::SegmentDistToSegmentSafe(From, To, V->GetActorLocation() - FVector(0.f, 0.f, V->GetSimpleCollisionHalfHeight()), V->GetActorLocation() + FVector(0.f, 0.f, V->GetSimpleCollisionHalfHeight()), OnLine, OnBody);
		if (FVector::Dist(OnLine, OnBody) < V->GetSimpleCollisionRadius() + Boss->Tune(TEXT("boltWidth"), 90.f) && FVector::Dist(From, OnLine) > WRTune(TEXT("creatureAI.storm.minRanged"), 400.f))
		{
			Boss->StrikeHit(V);
		}
	}
}

bool FWRStormBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	const FName Id = Attack.Id;
	const float Active = Attack.bRow ? FMath::Max(0.1f, Attack.Row.Active) : 0.25f;
	if (Id == TEXT("bell_toll"))
	{
		// He shakes his head: a 10 m ring of sound drains 40 stamina from everyone it reaches (a block doesn't stop it).
		const float Radius = Boss->Tune(TEXT("tollRadius"), 1000.f);
		WRFx::Broadcast(Boss, TEXT("storm.bellring"), SbFeet(Boss), FRotator::ZeroRotator, Radius / 1000.f, TEXT("storm.bell_toll"));
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			if (FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) > Radius + V->GetSimpleCollisionRadius()) continue;
			if (UAbilitySystemComponent* ASC = V->GetAbilitySystemComponent())
			{
				if (const UWRAttributeSet* A = V->GetAttributes())
				{
					ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(-20.f, A->GetStamina() - Boss->Tune(TEXT("tollStamina"), 40.f)));
					V->Status->NotifyStaminaUsed();
				}
			}
		}
		// Once a fight below 60%, the toll calls the herd: two more Thunderhorns within 20 s.
		const UWRAttributeSet* A = Boss->GetAttributes();
		if (!St.bHerdCalled && A && A->GetHealth() < A->GetMaxHealth() * Boss->Tune(TEXT("herdCallAt"), 0.6f))
		{
			St.bHerdCalled = true;
			AWRCharacterBase* Foe = Boss->AttackTarget.Get();
			for (int32 i = 0; i < FMath::RoundToInt(Boss->Tune(TEXT("herdCallCount"), 2.f)); ++i)
			{
				bool bOk = false;
				const FVector Spot = SbGround(Boss->GetWorld(), Boss->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * Boss->Tune(TEXT("herdCallDistance"), 4000.f), bOk);
				if (!bOk) continue;
				if (AWRCreature* Ram = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("thunderhorn"), Spot, FRotator::ZeroRotator))
				{
					Ram->SetAggroTarget(Foe);
					St.Herd.Add(Ram);
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Bellwether's toll calls the herd"));
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("kite_call"))
	{
		// A shriek: two Stormkites dive from the cloud.
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector At = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->Tune(TEXT("coreDrop"), 800.f)) + FRotator(0.f, 180.f * i + 90.f, 0.f).Vector() * 700.f;
			if (AWRCreature* Kite = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("stormkite"), At, Boss->GetActorRotation()))
			{
				Kite->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				Kite->SetAggroTarget(Boss->AttackTarget.Get());
				St.Kites.Add(Kite);
			}
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("tail_lash_pass"))
	{
		Boss->StrikeFrom = Boss->GetActorLocation();
		Boss->StrikeTo = St.LashTo;
		Boss->StrikeEnds = T + Active;
		Boss->MulticastTell(1, TEXT("tail"), TEXT("storm.arc"));
		return true;
	}
	if (Id == TEXT("gust_beat"))
	{
		// A wingbeat: a 40 deg cone, 25 m, pushes 700 cm toward the nearest edge; bracing (holding Block) halves it.
		WRFx::Broadcast(Boss, TEXT("storm.gustbeat"), SbFeet(Boss) + FVector(0.f, 0.f, 200.f), Boss->AttackFacing, Attack.Range / 1000.f, TEXT("storm.gust"));
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			const FVector To = V->GetActorLocation() - Boss->GetActorLocation();
			const float Bearing = FMath::Abs(FMath::FindDeltaAngleDegrees(Boss->AttackFacing.Yaw, To.Rotation().Yaw));
			if (To.Size2D() > Attack.Range || Bearing > Attack.Size) continue;
			const FVector Edge = (V->GetActorLocation() - Boss->GetHome()).GetSafeNormal2D();
			const float K = Boss->Tune(TEXT("gustPush"), 700.f) * (V->HasTag(WRTags::State_Blocking) ? Boss->Tune(TEXT("gustBraceMult"), 0.5f) : 1.f) * WRArtifacts::KnockScale(V);
			V->LaunchCharacter((Edge.IsNearlyZero() ? To.GetSafeNormal2D() : Edge) * K + FVector(0.f, 0.f, 120.f), true, false);
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("tunnel"))
	{
		// He dives: under the scree (3 to 6 s), a ridge of soil showing his way.
		St.bUnder = true;
		St.SurfaceBy = T + FMath::FRandRange(Boss->Tune(TEXT("underMin"), 3.f), Boss->Tune(TEXT("underMax"), 6.f));
		WRFx::Broadcast(Boss, TEXT("burst"), SbFeet(Boss), FRotator::ZeroRotator, 2.f, TEXT("storm.burrow"));
		Boss->SetActorHiddenInGame(true);
		Boss->SetActorEnableCollision(false);
		Boss->GetCharacterMovement()->DisableMovement();
		if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Set(WRStormFlag::Underground, true);
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("ring_pulse"))
	{
		St.Rings = FMath::RandRange(1, FMath::RoundToInt(Boss->Tune(TEXT("ringsMax"), 3.f)));
		St.RingsStarted = 0;
		St.RingStart = T;
		Boss->StrikeEnds = T + Boss->Tune(TEXT("ringGap"), 0.6f) * (St.Rings - 1) + Boss->ArenaRadius / Boss->Tune(TEXT("ringSpeed"), 900.f);
		return true;
	}
	if (Id == TEXT("surface_burst"))
	{
		// He erupts: the 6 m circle under his target.
		Surface(Boss, Boss->AttackPoint);
		WRFx::Broadcast(Boss, TEXT("burst"), Boss->AttackPoint, FRotator(90.f, 0.f, 0.f), Attack.Size / 120.f, TEXT("stomp"));
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			if (FVector::Dist2D(V->GetActorLocation(), Boss->AttackPoint) < Attack.Size + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("scree_spray"))
	{
		// He digs and flings five stones in a 30 deg fan at 1,200 cm/s: never homing, never inside 4 m.
		const FVector From = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() + FVector(0.f, 0.f, 40.f);
		const FVector Aim = Boss->AttackPoint + FVector(0.f, 0.f, 60.f);
		const int32 N = FMath::Max(1, Attack.Repeats > 1 ? Attack.Repeats : 5);
		const float Fan = Boss->Tune(TEXT("screeFan"), 30.f);
		const float Speed = FMath::Min(Attack.Row.Speed > 0.f ? Attack.Row.Speed : 1200.f, WRTune(TEXT("creatureAI.storm.maxProjectileSpeed"), 1400.f));
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Dir = (Aim - From).GetSafeNormal().RotateAngleAxis(-Fan * 0.5f + Fan * i / FMath::Max(1, N - 1), FVector::UpVector);
			FActorSpawnParameters Params;
			Params.Owner = Boss;
			Params.Instigator = Boss;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRProjectile* Stone = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir.Rotation(), Params))
			{
				FWRHit Hit;
				Hit.Instigator = Boss;
				Hit.Damage = Attack.Damage * SbScale();
				Hit.PoiseDamage = Attack.Poise;
				Hit.Impact = Attack.Impact;
				Hit.bProjectile = true;
				Hit.PhysType = 2;
				Hit.Element = WRTags::Element_Physical;
				Stone->Init(Hit, Speed, FLinearColor(0.42f, 0.38f, 0.33f));
			}
		}
		WRFx::Broadcast(Boss, TEXT("burst"), From, Boss->GetActorRotation(), 0.6f, TEXT("storm.burrow"));
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("talon_dive") || Id == TEXT("sky_dive"))
	{
		// From the fold, a straight line at the spot (no tracking): talons on arrival, then the ring.
		Boss->StrikeFrom = Boss->GetActorLocation();
		Boss->StrikeTo = St.Aim + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 20.f);
		Boss->StrikeEnds = T + FMath::Clamp(float(FVector::Dist(Boss->StrikeFrom, Boss->StrikeTo)) / Boss->Tune(TEXT("diveSpeed"), 4500.f), 0.3f, 1.0f);
		St.bImpacted = false;
		St.bDoubleRing = St.bArmed;
		if (St.bArmed) { St.bArmed = false; St.Charge = 0.f; }
		WRSound::Voice(Boss, TEXT("attack"));
		return true;
	}
	if (Id == TEXT("bone_drop"))
	{
		// A low swoop along a straight line: the first body it meets is carried off.
		const FVector Dir = Boss->AttackFacing.Vector().GetSafeNormal2D();
		Boss->SetAirborne(true);
		bool bOk = false;
		const FVector G = SbGround(Boss->GetWorld(), Boss->GetActorLocation(), bOk);
		Boss->StrikeFrom = FVector(Boss->GetActorLocation().X, Boss->GetActorLocation().Y, G.Z + Boss->GetSimpleCollisionHalfHeight() + 60.f);
		Boss->StrikeTo = Boss->StrikeFrom + Dir * Attack.Range * 0.9f;
		Boss->StrikeEnds = T + FMath::Clamp(Attack.Range * 0.9f / Boss->Tune(TEXT("swoopSpeed"), 2600.f), 0.3f, 1.0f);
		return true;
	}
	if (Id == TEXT("rod_chain"))
	{
		for (const auto& P : St.Pairs)
		{
			if (P.Key.IsValid() && P.Value.IsValid()) P.Key->MulticastArc(P.Value->Tip(), Active);
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("eye_bolt") || Id == TEXT("double_eye_bolt"))
	{
		const FVector From = EyePoint(Boss);
		const FVector End = BoltEnd(Boss, From, St.Aim);
		Stroke(Boss, From, End);
		if (St.bArmed)
		{
			// Charged to the full: it forks to two more within 8 m.
			St.bArmed = false;
			St.Charge = 0.f;
			int32 Forks = 0;
			for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
			{
				if (Forks >= 2 || FVector::Dist(V->GetActorLocation(), End) > Boss->Tune(TEXT("forkRadius"), 800.f) || Boss->StrikeHits.Contains(V)) continue;
				Stroke(Boss, End, V->GetActorLocation());
				++Forks;
			}
		}
		Boss->StrikeEnds = T + (Id == TEXT("double_eye_bolt") ? Boss->Tune(TEXT("doubleBoltGap"), 0.6f) + Active : Active);
		return true;
	}
	if (Id == TEXT("storm_pillar"))
	{
		WRFx::Broadcast(Boss, TEXT("storm.pillar"), FVector(Boss->GetActorLocation().X, Boss->GetActorLocation().Y, St.Aim.Z), FRotator(90.f, 0.f, 0.f), Attack.Size / 100.f, TEXT("storm.strike"));
		Boss->StrikeEnds = T + Active;
		return true;   // its column is live through the active window (TickStrike)
	}
	if (Id == TEXT("thunderclap"))
	{
		// He claps his wings together over the arena: a 12 m ring at 1,200 cm/s from the floor below him, launching.
		bool bOk = false;
		Boss->StrikeTo = SbGround(Boss->GetWorld(), Boss->GetActorLocation(), bOk, 500.f, 6000.f);
		WRFx::Broadcast(Boss, TEXT("shockwave"), Boss->StrikeTo, FRotator::ZeroRotator, Attack.Size / 100.f, TEXT("storm.clap"));
		WRFx::Broadcast(Boss, TEXT("storm.clap"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("storm.thunder"));
		Boss->StrikeEnds = T + Attack.Size / WRTune(TEXT("bosses.waveSpeed"), 1200.f);
		return true;
	}
	return false;
}

bool FWRStormBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	const FName Id = Attack.Id;
	const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
	const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
	if (Id == TEXT("bell_toll") || Id == TEXT("kite_call") || Id == TEXT("gust_beat") || Id == TEXT("tunnel") || Id == TEXT("surface_burst") || Id == TEXT("scree_spray"))
	{
		return true;
	}
	if (Id == TEXT("tail_lash_pass"))
	{
		// Along the line at 3 m, the tail sparking: whatever it passes takes it (jump or roll).
		Boss->SetActorLocation(FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A), false);
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			if (FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetSimpleCollisionRadius() + V->GetSimpleCollisionRadius() + Attack.Size * 0.5f
				&& FMath::Abs(V->GetActorLocation().Z - (Boss->GetActorLocation().Z - Boss->GetSimpleCollisionHalfHeight() - 200.f)) < 400.f)
			{
				Boss->StrikeHit(V);
			}
		}
		return true;
	}
	if (Id == TEXT("ring_pulse"))
	{
		// 1 to 3 rings of current spread from his spot at 900 cm/s, half a metre tall (jump them).
		const float Gap = Boss->Tune(TEXT("ringGap"), 0.6f), Speed = Boss->Tune(TEXT("ringSpeed"), 900.f);
		const int32 Started = FMath::Min(St.Rings, 1 + FMath::FloorToInt(float(T - St.RingStart) / Gap));
		while (St.RingsStarted < Started)
		{
			Boss->StrikeHits.Reset();   // each ring is its own blow
			WRFx::Broadcast(Boss, TEXT("storm.ringpulse"), St.RingAt + FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, Boss->ArenaRadius / 2750.f, TEXT("storm.ground_current"));
			++St.RingsStarted;
		}
		const int32 Ring = St.RingsStarted - 1;
		const float Front = float(T - St.RingStart - Gap * Ring) * Speed;
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			const float D = FVector::Dist2D(V->GetActorLocation(), St.RingAt) - V->GetSimpleCollisionRadius();
			if (Front >= 0.f && FMath::Abs(D - Front) < Boss->Tune(TEXT("ringDepth"), 110.f) && FMath::Abs(SbFeet(V).Z - St.RingAt.Z) < 250.f) Boss->StrikeHit(V);
		}
		return true;
	}
	if (Id == TEXT("talon_dive") || Id == TEXT("sky_dive"))
	{
		if (!St.bImpacted)
		{
			Boss->SetActorLocation(FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A * A), false);
			if (A >= 1.f)
			{
				// Talons in (unblockable), then the dust-and-spark ring at 1,200 cm/s, 6 m, low; doubled when charged.
				St.bImpacted = true;
				St.ImpactAt = T;
				Boss->SetAirborne(false);
				if (St.Shadow.IsValid()) St.Shadow->Destroy();
				for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
				{
					if (FVector::Dist2D(V->GetActorLocation(), St.Aim) < Attack.Size + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
				}
				WRFx::Broadcast(Boss, TEXT("shockwave"), St.Aim, FRotator::ZeroRotator, Boss->Tune(TEXT("diveRing"), 600.f) / 100.f, TEXT("stomp"));
				WRFx::Broadcast(Boss, TEXT("status.shock"), St.Aim + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 3.f, TEXT("storm.thunder"));
				Boss->StrikeHits.Reset();
				Boss->StrikeEnds = T + Boss->Tune(TEXT("diveRing"), 600.f) / 1200.f + (St.bDoubleRing ? 0.4f : 0.f) + 0.05f;
			}
			return true;
		}
		// The ring(s) on the floor.
		for (int32 Ring = 0; Ring < (St.bDoubleRing ? 2 : 1); ++Ring)
		{
			const float Front = float(T - St.ImpactAt - Ring * 0.4) * 1200.f;
			if (Ring == 1 && !St.bSecondFired && Front >= 0.f)
			{
				St.bSecondFired = true;
				WRFx::Broadcast(Boss, TEXT("shockwave"), St.Aim, FRotator::ZeroRotator, Boss->Tune(TEXT("diveRing"), 600.f) / 100.f, TEXT("storm.thunder"));
			}
			for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
			{
				const float D = FVector::Dist2D(V->GetActorLocation(), St.Aim) - V->GetSimpleCollisionRadius();
				if (Front >= 0.f && D <= Boss->Tune(TEXT("diveRing"), 600.f) && FMath::Abs(D - Front) < 120.f && !Boss->StrikeHits.Contains(V))
				{
					if (Ring == 1 || !St.bDoubleRing) Boss->StrikeHits.Add(V);
					if (!V->HasTag(WRTags::State_JumpLegsInvuln)) SbHitWith(Boss, V, Boss->Tune(TEXT("diveRingDamage"), 35.f), 0.f, false, true, 3);
				}
			}
		}
		return true;
	}
	if (Id == TEXT("bone_drop"))
	{
		Boss->SetActorLocation(FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A), false);
		if (!St.Carried.IsValid())
		{
			for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
			{
				if (FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetSimpleCollisionRadius() + V->GetSimpleCollisionRadius() + 80.f) { Boss->StrikeHit(V); break; }
			}
		}
		return true;
	}
	if (Id == TEXT("rod_chain"))
	{
		// The arcs jump straight across between each pair, low over the floor: step off the lines, or roll through.
		for (const auto& P : St.Pairs)
		{
			if (!P.Key.IsValid() || !P.Value.IsValid()) continue;
			const FVector From = P.Key->GetActorLocation(), To = P.Value->GetActorLocation();
			for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
			{
				if (SbSegDist2D(V->GetActorLocation(), From, To) < V->GetSimpleCollisionRadius() + Boss->Tune(TEXT("rodChainWidth"), 80.f) && SbFeet(V).Z - FMath::Min(From.Z, To.Z) < 300.f) Boss->StrikeHit(V);
			}
		}
		return true;
	}
	if (Id == TEXT("double_eye_bolt") && !St.bSecondFired)
	{
		// A second flare after the first stroke: the second bolt re-aims once, boltLead s before it fires, at where you went.
		const float Gap = Boss->Tune(TEXT("doubleBoltGap"), 1.0f);
		if (St.Stage == 1 && T - Boss->StrikeStarted >= FMath::Max(0.f, Gap - Boss->Tune(TEXT("boltLead"), 0.9f)))
		{
			St.Stage = 2;
			AWRCharacterBase* Target = Boss->AttackTarget.Get();
			St.Aim2 = Target ? Target->GetActorLocation() : St.Aim;
			Boss->MulticastTell(1, TEXT("Head"), TEXT("tell.final"));
			SbLine(Boss, TEXT("storm.boltleader"), EyePoint(Boss), BoltEnd(Boss, EyePoint(Boss), St.Aim2), TEXT("storm.spark"));
		}
		if (St.Stage == 2 && T - Boss->StrikeStarted >= Gap)
		{
			St.bSecondFired = true;
			Boss->StrikeHits.Reset();
			const FVector From = EyePoint(Boss);
			Stroke(Boss, From, BoltEnd(Boss, From, St.Aim2));
		}
		return true;
	}
	if (Id == TEXT("eye_bolt") || Id == TEXT("double_eye_bolt"))
	{
		return true;
	}
	if (Id == TEXT("storm_pillar"))
	{
		// A 5 m column straight down under him.
		for (AWRCharacterBase* V : SbFoes(Boss, Boss->ChaseRadius))
		{
			if (FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) < Attack.Size + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
		}
		return true;
	}
	(void)DeltaSeconds;
	return false;   // the thunderclap's ring rolls on the boss machine's own wave
}

void FWRStormBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	const FName Id = Attack.Id;
	if (Id == TEXT("rod_chain"))
	{
		for (const auto& P : St.Pairs)
		{
			if (P.Key.IsValid() && P.Key->State == 1) P.Key->SetState(0);
			if (P.Value.IsValid() && P.Value->State == 1) P.Value->SetState(0);
		}
		St.Pairs.Reset();
	}
	else if (Id == TEXT("thunderclap"))
	{
		// The clap knocks down 4 of the 12 rim rods (their stumps stay); the storm thickens.
		TArray<AWRStormProp*> Standing = SbRods(Boss, true);
		for (int32 i = Standing.Num() - 1; i > 0; --i) Standing.Swap(i, FMath::RandRange(0, i));
		for (int32 i = 0; i < FMath::Min(Standing.Num(), FMath::RoundToInt(Boss->Tune(TEXT("rodsLostAtTransition"), 4.f))); ++i)
		{
			Standing[i]->SetState(3);
			WRFx::Broadcast(Boss, TEXT("part.break"), Standing[i]->Tip() - FVector(0.f, 0.f, Standing[i]->Size * 0.5f), FRotator::ZeroRotator, 2.f, TEXT("part.break"));
		}
		Boss->SetAirborne(false);
		St.FlightClock = SbNow(Boss);
		St.NextCalledAt = SbNow(Boss) + 2.0;
	}
	else if (Id == TEXT("talon_dive") || Id == TEXT("sky_dive"))
	{
		Boss->SetAirborne(false);
		St.FlightClock = SbNow(Boss);
	}
	else if (Id == TEXT("sky_buffet"))
	{
		Boss->SetAirborne(true);   // the sky combo ends with him taking off
		St.FlightClock = SbNow(Boss);
	}
	else if (Id == TEXT("bone_drop") && !St.Carried.IsValid() && Boss->Phase < 3)
	{
		Boss->SetAirborne(false);
	}
	else if (Id == TEXT("tail_lash_pass"))
	{
		Boss->LaunchCharacter(FVector(0.f, 0.f, 1200.f), false, true);   // back up into her cloud
	}
}

void FWRStormBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	if (Hit.Buildups.Contains(TEXT("shock"))) Hit.Element = WRTags::Element_Lightning;
	(void)Attack;
}

void FWRStormBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsStormBoss(Boss) || !Victim)
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (Boss->CreatureId == TEXT("kharvox") && bLanded && SbNow(Boss) >= St.NoGainUntil)
	{
		St.Charge += Boss->Tune(TEXT("chargePerHit"), 3.f);
	}
	if (AttackId == TEXT("bone_drop") && Result == EWRHitResult::Hit && Victim->IsAlive() && !St.Carried.IsValid())
	{
		// Up it goes: 15 m (a tame 25 m), toward the ossuary slab.
		St.Carried = Victim;
		St.CarryStart = SbNow(Boss);
		St.HeadStance = 0.f;
		St.CarryUp = Victim->IsCreature() ? Boss->Tune(TEXT("boneDropTameHeight"), 2500.f) : Boss->Tune(TEXT("boneDropHeight"), 1500.f);
		Victim->SetActorEnableCollision(false);
		Victim->GetCharacterMovement()->DisableMovement();
		Victim->AttachToActor(Boss, FAttachmentTransformRules::KeepWorldTransform);
		Victim->SetActorRelativeLocation(FVector(Boss->GetSimpleCollisionRadius() * 0.2f, 0.f, -Boss->GetSimpleCollisionHalfHeight() - Victim->GetSimpleCollisionHalfHeight()));
		Victim->Status->Stagger(Boss->Tune(TEXT("boneDropMaxCarry"), 4.f) + 0.5f);
		Boss->StrikeEnds = SbNow(Boss);
		if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Set(WRStormFlag::Grounded, false);
		UE_LOG(LogWildreach, Log, TEXT("BOSS Kharvox carries %s off"), *Victim->GetName());
	}
	if (AttackId == TEXT("ridge_charge") && bLanded && Victim->IsAlive())
	{
		// Knocked 900 cm sideways (off a ledge if there is one).
		const FVector Fwd = Boss->AttackFacing.Vector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd) * (FVector::DotProduct(Victim->GetActorLocation() - Boss->GetActorLocation(), FVector::CrossProduct(FVector::UpVector, Fwd)) >= 0.f ? 1.f : -1.f);
		const float K = Boss->Tune(TEXT("chargeKnock"), 900.f) * WRArtifacts::KnockScale(Victim);
		Victim->LaunchCharacter(Side * K + Fwd * K * 0.3f + FVector(0.f, 0.f, K * 0.3f), true, true);
	}
	if (AttackId == TEXT("thunderclap_butt") && bLanded)
	{
		WRFx::Broadcast(Boss, TEXT("storm.clap"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("storm.clap"));
	}
}

float FWRStormBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsStormBoss(Boss))
	{
		return Damage;
	}
	const FWRStormBossState* St = SbFind(Boss);
	const double T = SbNow(Boss);
	// Weak to Blood and curse (x1.5): a Blood path blow, or one carrying bleed or curse.
	if (Hit.PathId == TEXT("blood") || Hit.Buildups.FindRef(TEXT("bleed")) > 0.f || Hit.Buildups.FindRef(TEXT("curse")) > 0.f)
	{
		Damage *= WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f);
	}
	if (St)
	{
		if (T < St->StuckUntil) Damage *= Boss->Tune(TEXT("stuckDamageTaken"), 1.5f);
		if (T < St->DropUntil) Damage *= Boss->Tune(TEXT("dropDamageTaken"), 1.5f);
		if (T < St->CursedUntil) Damage *= Boss->Tune(TEXT("curseDamageTaken"), 1.25f);
	}
	return Damage;
}

bool FWRStormBoss::OwnWeakness(const AWRBoss* Boss)
{
	return IsStormBoss(Boss);
}

void FWRStormBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsStormBoss(Boss) || Hit.bDamageOverTime)
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	const FVector At = Hit.ImpactPoint.IsZero() ? FVector::ZeroVector : Hit.ImpactPoint;
	if (At.IsZero())
	{
		return;
	}
	// Parts with their own health (a bell 400, a star nose 300): the blows that land on them.
	const FWRCreatureDef* Def = Boss->GetDef();
	if (!Def)
	{
		return;
	}
	for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
	{
		const FWRCreatureDef::FBodyPoint& P = Def->Parts[i];
		float* HP = P.OnBreak == TEXT("bell") ? &St.BellHP : P.OnBreak == TEXT("nose") ? &St.NoseHP : nullptr;
		if (!HP || Boss->IsPartBroken(P.Id) || FVector::Dist(Boss->BodyPointLocation(P), At) > WRTune(TEXT("stance.partRadius"), 120.f) + Boss->GetSimpleCollisionRadius() * 0.2f) continue;
		*HP -= Damage;
		if (*HP <= 0.f)
		{
			Boss->PartsBroken |= uint8(1 << i);
			Boss->OnRep_PartsBroken();
			UE_LOG(LogWildreach, Log, TEXT("PART %s's %s broken"), *Boss->GetDisplayLabel(), *P.Id.ToString());
			Boss->OnPartBroken(P, Hit.Instigator);
		}
	}
}

void FWRStormBoss::PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	if (!IsStormBoss(Boss))
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	if (Part.OnBreak == TEXT("bell"))
	{
		// No more tolls or herd calls, and a 3 s stagger.
		St.bHerdCalled = true;
		Boss->Status->Stagger(Boss->Tune(TEXT("bellBreakStagger"), 3.f));
		WRSound::PlayOn(Boss, TEXT("storm.bell_toll"));
	}
	else if (Part.OnBreak == TEXT("nose"))
	{
		St.BlindUntil = T + Boss->Tune(TEXT("noseBlindSeconds"), 20.f);
	}
	else if (Part.OnBreak == TEXT("wing_ground"))
	{
		// A wing broken grounds him for 6 s (once per wing per fight; its Storm plume goes to the fight).
		Boss->SetAirborne(false);
		Boss->GroundedUntil = FMath::Max(Boss->GroundedUntil, T + Boss->Tune(TEXT("wingGroundSeconds"), 6.f));
		Boss->Status->Stagger(1.f);
		WRFx::Broadcast(Boss, TEXT("stomp"), SbFeet(Boss), FRotator::ZeroRotator, 2.f, TEXT("stomp"));
	}
	(void)By;
}

void FWRStormBoss::WallStun(AWRBoss* Boss, float Seconds)
{
	if (Boss && Boss->CreatureId == TEXT("bellwether"))
	{
		// Horns stuck in the rock: 5 s, x1.5 damage and x2 stance taken.
		SbS(Boss).StuckUntil = SbNow(Boss) + Seconds;
		WRSound::PlayOn(Boss, TEXT("storm.bell_toll"));
	}
}

void FWRStormBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!Boss || Boss->CreatureId != TEXT("kharvox"))
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	if (NewPhase == 2)
	{
		St.NextCalledAt = SbNow(Boss) + 2.0;
	}
	else if (NewPhase >= 3)
	{
		// The eye of the storm: every remaining rod melts in one burst; strikes every 2 s; he flies.
		for (AWRStormProp* Rod : SbRods(Boss, true))
		{
			Rod->SetState(2);
			WRFx::Broadcast(Boss, TEXT("weather.strike"), Rod->Tip(), FRotator::ZeroRotator, 1.f, TEXT("storm.strike"));
		}
		St.EyeAt = Boss->GetHome();
		Boss->SetAirborne(true);
		St.FlightClock = SbNow(Boss);
		St.NextCalledAt = SbNow(Boss) + 2.0;
	}
	else if (NewPhase == 1)
	{
		if (UWRStormBody* Body = UWRStormBody::Get(Boss)) Body->Set(WRStormFlag::Eye, false);
		St.EyeAt = FVector::ZeroVector;
	}
}

bool FWRStormBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!IsStormBoss(Boss))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Row = SbArena(Boss->CreatureId);
	FVector At;
	if (!SbMetres(Row, TEXT("runback"), At))
	{
		return Boss->CreatureId == TEXT("bellwether");   // roaming: no shrine-stone (as the island's roamers)
	}
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
	{
		if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	}
	bool bOk = false;
	const FVector G = SbGround(Boss->GetWorld(), At, bOk, 3000.f, 6000.f);
	if (!bOk)
	{
		return false;   // not streamed in: the boss machine tries its own way
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Face(0.f, (Boss->GetHome() - G).Rotation().Yaw, 0.f);
	if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), G, Face, Params))
	{
		Stone->BossId = Boss->CreatureId;
		Boss->Runback = Stone;
		UE_LOG(LogWildreach, Log, TEXT("BOSS %s shrine-stone at %s"), *Boss->GetDisplayLabel(), *G.ToCompactString());
	}
	return true;
}

FName FWRStormBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == TEXT("kharvox"))
	{
		return bScarred ? FName(TEXT("part_stormheart_scarred")) : FName(TEXT("part_stormheart"));
	}
	return Fallback;
}

void FWRStormBoss::StrikeLanded(UWorld* World, const FWRStrikeCall& Call)
{
	AWRBoss* Boss = Cast<AWRBoss>(Call.Instigator.Get());
	if (!Boss || Boss->CreatureId != TEXT("kharvox") || !Boss->IsAlive())
	{
		return;
	}
	FWRStormBossState& St = SbS(Boss);
	if (SbNow(Boss) >= St.NoGainUntil) St.Charge += Boss->Tune(TEXT("chargePerStrike"), 10.f);
	(void)World;
}

float FWRStormBoss::StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance)
{
	AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!IsStormBoss(Boss))
	{
		return 1.f;
	}
	FWRStormBossState& St = SbS(Boss);
	const double T = SbNow(Boss);
	float Mult = 1.f;
	if (T < St.StuckUntil) Mult *= Boss->Tune(TEXT("stuckStanceTaken"), 2.f);
	if (Boss->CreatureId == TEXT("galvara") && !Boss->bAirborne)
	{
		St.StanceLog.Add({ T, Stance * Mult });
	}
	if (Boss->CreatureId == TEXT("kharvox") && St.Carried.IsValid() && (Hit.ImpactPoint.IsZero() || Boss->IsNearLockPoint(Hit.ImpactPoint, TEXT("head"), Boss->GetSimpleCollisionRadius() * 0.5f)))
	{
		// Allies free a grabbed friend: 150 stance on the head during the climb.
		St.HeadStance += Stance * Mult;
		if (St.HeadStance >= Boss->Tune(TEXT("boneDropFreeStance"), 150.f))
		{
			ReleaseCarried(Boss, true);
			Boss->Status->Stagger(1.2f);
		}
	}
	return Mult;
}

// ---------------------------------------------------------------- living-world beats (creatures)

bool FWRStormBoss::TickBeat(AWRCreatureAI* AI, AWRCreature* Self, UWRStormBody* Body, float DeltaSeconds)
{
	AWRCreature* Other = Body->Partner.Get();
	const double T = SbNow(Self);
	if (!Other || !Other->IsAlive() || Self->GetAggroTarget())
	{
		Body->BeatUntil = 0.0;
		Body->Partner = nullptr;
		return false;
	}
	if (Body->BeatStep <= 1)
	{
		// The Rams' Ring clash: square off across the ring, rear, run together, the crack; a pair fighting ignores you
		// unless you come within 10 m.
		for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < WRTune(TEXT("creatureAI.storm.clashIgnoreRadius"), 1000.f))
			{
				Body->BeatUntil = 0.0;
				Self->SetAggroTarget(*It);
				return false;
			}
		}
		const FVector Ring = Body->ScoutAt;
		const FVector Axis = FVector(1.f, 0.f, 0.f);
		const float Side = Body->BeatStep == 0 ? -1.f : 1.f;
		const float Phase = FMath::Fmod(float(T), 7.f);
		const float Apart = Phase < 4.f ? 600.f : Phase < 5.2f ? 600.f - (Phase - 4.f) / 1.2f * 480.f : 120.f;
		const FVector Spot = Ring + Axis * Side * Apart;
		const FVector To = Spot - Self->GetActorLocation();
		Self->SetAsleep(false);
		if (To.Size2D() > 60.f) Self->AddMovementInput(To.GetSafeNormal2D(), Phase >= 4.f && Phase < 5.2f ? 1.f : 0.5f);
		AI->SetControlRotation((Other->GetActorLocation() - Self->GetActorLocation()).Rotation());
		Self->SetActorRotation(FMath::RInterpConstantTo(Self->GetActorRotation(), FRotator(0.f, (Other->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f), DeltaSeconds, 120.f));
		if (Phase >= 3.2f && Phase < 3.2f + DeltaSeconds * 1.5f) Self->PlaySwing(EWRSwingKind::Strike, 0.8f);   // rearing
		if (Body->BeatStep == 0 && Phase >= 5.2f && Phase < 5.2f + DeltaSeconds * 1.5f)
		{
			const FVector Mid = (Self->GetActorLocation() + Other->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() * 0.6f);
			WRFx::Broadcast(Self, TEXT("storm.clap"), Mid, FRotator::ZeroRotator, 1.5f, TEXT("storm.clap"));
			WRSound::Broadcast(Self, TEXT("storm.thunder"));
		}
		return true;
	}
	// A kite's snatch: down on the Rodcrest, up 15 m with it, and let go.
	if (Body->BeatStep == 10)
	{
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector To = Other->GetActorLocation() + FVector(0.f, 0.f, Other->GetSimpleCollisionHalfHeight()) - Self->GetActorLocation();
		Self->AddMovementInput(To.GetSafeNormal(), 1.f);
		if (To.Size() < Self->GetSimpleCollisionRadius() + Other->GetSimpleCollisionRadius() + 80.f)
		{
			Body->BeatStep = 11;
			Body->ScoutUntil = T;
			if (UWRStormBody* Prey = UWRStormBody::Get(Other)) Prey->SnatchedBy = Self;
			Other->SetActorEnableCollision(false);
			Other->GetCharacterMovement()->DisableMovement();
			Other->AttachToActor(Self, FAttachmentTransformRules::KeepWorldTransform);
			Other->SetActorRelativeLocation(FVector(0.f, 0.f, -Self->GetSimpleCollisionHalfHeight() - Other->GetSimpleCollisionHalfHeight()));
			WRSound::Voice(Self, TEXT("attack"));
		}
		return true;
	}
	if (Body->BeatStep == 11)
	{
		Self->AddMovementInput(FVector(0.f, 0.f, 1.f), 1.f);
		if (T - Body->ScoutUntil > WRTune(TEXT("creatureAI.storm.snatchCarry"), 2.5f))
		{
			Body->BeatStep = 12;
			Body->BeatUntil = 0.0;
			Other->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Other->SetActorEnableCollision(true);
			Other->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			if (UWRStormBody* Prey = UWRStormBody::Get(Other)) Prey->SnatchedBy = nullptr;
			Other->StartFleeing(Self, TEXT("dropped by a kite"));
			UE_LOG(LogWildreach, Log, TEXT("STORM %s drops %s"), *Self->GetName(), *Other->GetName());
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- placing the arenas

bool UWRStormArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRStormArenas::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client || !SbArena(TEXT("kharvox")).IsValid())
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UWRStormArenas::Step), 5.f, true);
}

void UWRStormArenas::Step()
{
	static const FName Ids[] = { TEXT("galvara"), TEXT("coilback"), TEXT("kharvox") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		const TSharedPtr<FJsonObject> Row = SbArena(Id);
		if (!Row.IsValid() || !FWRCreatureRegistry::Get().Find(Id)) { Placed.Add(Id); continue; }
		if (Place(Id, Row)) Placed.Add(Id);
	}
	if (Placed.Num() >= UE_ARRAY_COUNT(Ids))
	{
		GetWorld()->GetTimerManager().ClearTimer(StepTimer);
	}
}

bool UWRStormArenas::Place(FName Id, const TSharedPtr<FJsonObject>& Row)
{
	UWorld* World = GetWorld();
	const FVector At = SbCentre(Row);
	// Only once its ground is in (a survivor near enough to stream it) and only once a world.
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), At) < WRTune(TEXT("bosses.storm.placeRadius"), 30000.f);
	bool bOk = false;
	const FVector G = SbGround(World, FVector(At.X, At.Y, At.Z), bOk, 6000.f, 12000.f);
	if (!bNear || !bOk)
	{
		return false;
	}
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It)
	{
		if (It->CreatureId == Id) return true;   // already here
	}
	// The boss's spawner (as the island's: it brings the boss back server.bossRespawnHours after it falls).
	const FTransform Where(FRotator::ZeroRotator, G + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = Id;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = true;
		Spawner->FinishSpawning(Where);
	}
	const auto Prop = [World](FName Kind, const FVector& P, float Size, float Yaw) -> AWRStormProp*
	{
		bool bGround = false;
		const FVector PG = SbGround(World, P, bGround, 6000.f, 12000.f);
		const FTransform T(FRotator(0.f, Yaw, 0.f), PG);
		AWRStormProp* A = World->SpawnActorDeferred<AWRStormProp>(AWRStormProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Kind = Kind;
			A->Size = Size;
			A->FinishSpawning(T);
		}
		return A;
	};
	if (Id == TEXT("kharvox"))
	{
		// Twelve old Rodwarden rods round the rim, 8 m tall; the nest of trunks and stolen rods; the ossuary slab and the
		// bronze rod-altar beside it (the Lightning altar).
		const int32 N = FMath::RoundToInt(Row->HasField(TEXT("rods")) ? Row->GetNumberField(TEXT("rods")) : 12.0);
		const float Radius = Row->HasField(TEXT("rodRadius")) ? float(Row->GetNumberField(TEXT("rodRadius"))) : 2600.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 360.f * i / N;
			Prop(TEXT("rim_rod"), G + FRotator(0.f, A, 0.f).Vector() * Radius, WRTune(TEXT("bosses.kharvox.rodHeight"), 800.f), A);
		}
		// (The nest and the ossuary slab are the summit landmark's own, built with the land: WRStormWorld's summit_nest.
		// AWRStormProp still draws both kinds for an arena that has no landmark.)
		FVector P;
		if (SbMetres(Row, TEXT("altar"), P))
		{
			bool bGround = false;
			const FVector AG = SbGround(World, FVector(P.X, P.Y, G.Z), bGround, 6000.f, 12000.f);
			const FTransform T(FRotator(0.f, (G - AG).Rotation().Yaw, 0.f), AG + FVector(0.f, 0.f, 50.f));
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("lightning");
				Altar->FinishSpawning(T);
			}
			Prop(TEXT("altar_rod"), AG, 300.f, (G - AG).Rotation().Yaw);
		}
	}
	else if (Id == TEXT("coilback"))
	{
		Prop(TEXT("veins"), G, WRTune(TEXT("bosses.coilback.veinReach"), 2600.f), 0.f);
	}
	UE_LOG(LogWildreach, Log, TEXT("STORM arena %s placed at %s"), *Id.ToString(), *G.ToCompactString());
	return true;
}
