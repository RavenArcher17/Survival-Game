// The Corrupted wasteland's bosses (see WRVoidBosses.h). Numbers: tuning bosses.<id>, attacks.json bosses.<id>.
#include "WRVoidBosses.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLandSites.h"
#include "WRPlayerController.h"
#include "WRPortalsInternal.h"
#include "WRProjectile.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRVoidCreatures.h"
#include "WRVoidPath.h"
#include "WRVoidWorld.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace WRVoidBossImpl
{
	const FLinearColor VbViolet = FLinearColor(FColor(0x7B, 0x4F, 0xD6));
	const FLinearColor VbDeep = FLinearColor(FColor(0x2B, 0x18, 0x40));
	const FLinearColor VbGlass = FLinearColor(0.004f, 0.0035f, 0.006f);
	const FLinearColor VbDolo = FLinearColor(FColor(201, 195, 184));

	double VbSNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
	double VbNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	float VbT(const AWRBoss* B, const TCHAR* Key, float F) { return B ? WRTune(*FString::Printf(TEXT("bosses.%s.%s"), *B->CreatureId.ToString(), Key), F) : F; }

	/** Per boss, the server's state for its counter. */
	struct FVbState
	{
		// The Unravelled.
		float SinceReform = 0.f;
		int32 UnravelStage = 0;          // 0 whole, 1 travelling, 2 core alone, 3 swirling in
		double StageUntil = 0.0;
		FVector TravelFrom = FVector::ZeroVector, TravelTo = FVector::ZeroVector;
		double TravelStart = 0.0;
		// The Mother: her pens, the open brood rifts.
		TArray<FVector> Pens;
		TArray<TWeakObjectPtr<AWRCreature>> Brood;
		TArray<double> BroodNext;
		bool bHadBrood = false;
		double StrainUntil = 0.0;
		// The Starving One.
		int32 Stacks = 0;
		double EatingUntil = 0.0;
		TWeakObjectPtr<AWRLootBag> Bait;
		// The Echo.
		TArray<FWRHit> Heard;            // the last path abilities that hit it, newest last
		int32 RouteIndex = 0;
		double FailUntil = 0.0;
		// Vorhollow.
		int32 ShellsDone = 0;
		float ShellHp = 0.f;
		double ShellUntil = 0.0;
		bool bForceBeam = false;
		double DiveRingAt = 0.0;
		FVector DiveAt = FVector::ZeroVector;
		bool bGulletOpen = false;
	};
	TMap<TWeakObjectPtr<const AWRBoss>, FVbState>& VbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FVbState> M;
		return M;
	}
	FVbState& VbSt(const AWRBoss* B) { return VbStates().FindOrAdd(B); }

	/** The land file's boss block (wasteland.json "bosses"). */
	TSharedPtr<FJsonObject> VbBossRow(FName Id)
	{
		for (const FWRLandDef& D : WRLand::All())
		{
			const TSharedPtr<FJsonObject> B = D.Section(TEXT("bosses"));
			const TSharedPtr<FJsonObject>* R = nullptr;
			if (B.IsValid() && B->TryGetObjectField(Id.ToString(), R)) return *R;
		}
		return nullptr;
	}
	FVector2D VbV2(const TSharedPtr<FJsonObject>& O, const TCHAR* F)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(F, A) && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : FVector2D::ZeroVector;
	}

	AWRCharacterBase* VbNearestPlayer(const AActor* From, float Range)
	{
		AWRCharacterBase* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AWRCharacter> It(From->GetWorld()); It; ++It)
		{
			if (!It->IsAlive()) continue;
			const float D = FVector::Dist(It->GetActorLocation(), From->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	void VbHurt(AActor* By, AWRCharacterBase* V, float Damage, uint8 Impact, bool bUnblockable, const FVector& From, float Curse = 0.f)
	{
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = Damage;
		Hit.Element = WRTags::Element_Void;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.Origin = From;
		if (Curse > 0.f) Hit.Buildups.Add(TEXT("curse"), Curse);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}

}
using namespace WRVoidBossImpl;

// (The boss machine's private parts are ours through the friend declaration: FWRVoidBoss.)

// ================================================================================================ the hooks

bool FWRVoidBoss::IsVoidBoss(const AWRBoss* Boss)
{
	const FWRCreatureDef* D = Boss ? Boss->GetDef() : nullptr;
	return D && D->Biome == TEXT("wasteland");
}

bool FWRVoidBoss::OwnWeakness(const AWRBoss* Boss)
{
	return IsVoidBoss(Boss);
}

void FWRVoidBoss::Begin(AWRBoss* Boss)
{
	if (!IsVoidBoss(Boss)) return;
	UWRVoidBody::AttachLook(Boss);
	if (!Boss->HasAuthority()) return;
	FVbState& S = VbSt(Boss);
	S = FVbState();
	const FVector Home = Boss->GetActorLocation();
	if (Boss->CreatureId == TEXT("mother_of_rifts"))
	{
		// Her six pens round the court (the Rift Pens' layout: a ring 17 m out), each a brood-rift socket.
		const TSharedPtr<FJsonObject> R = VbBossRow(Boss->CreatureId);
		const float Yaw = float(R.IsValid() ? R->GetNumberField(TEXT("yaw")) : 0.0);
		for (int32 p = 0; p < 6; ++p)
		{
			const float A = PI * 0.25f + PI * 1.5f * p / 5.f + FMath::DegreesToRadians(Yaw);
			const FVector P = Home + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 1700.f;
			S.Pens.Add(FVector(P.X, P.Y, WRLand::GroundZAt(P, P.Z)));
		}
		S.Brood.SetNum(6);
		S.BroodNext.SetNumZeroed(6);
	}
	if (Boss->CreatureId == TEXT("null_warden"))
	{
		// He's back: he re-pins the gate (it lowers over its seconds).
		if (AWRNullGate* G = AWRNullGate::Find(Boss->GetWorld())) G->SetOpen(false);
	}
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		S.ShellHp = 0.f;
		if (AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld())) { Stage->Split(false); Stage->OpenGullet(false); }
	}
}

void FWRVoidBoss::EndPlay(AWRBoss* Boss)
{
	if (!IsVoidBoss(Boss)) return;
	FVbState* S = VbStates().Find(Boss);
	if (S) for (const TWeakObjectPtr<AWRCreature>& C : S->Brood) if (C.IsValid()) C->Destroy();
	VbStates().Remove(Boss);
}

bool FWRVoidBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsVoidBoss(Boss) || Boss->CreatureId != TEXT("echo")) return false;
	// The Echo's loop: by night it walks its 2.4 km route at 3 m/s; by day it rests on the VbGlass Sea (the route's first point).
	const TSharedPtr<FJsonObject> R = VbBossRow(Boss->CreatureId);
	const TArray<TSharedPtr<FJsonValue>>* Route = nullptr;
	const FWRLandDef* D = WRLand::Find(TEXT("wasteland"));
	if (!R.IsValid() || !D || !R->TryGetArrayField(TEXT("route"), Route) || Route->Num() < 2) return false;
	FVbState& S = VbSt(Boss);
	const bool bNight = WRWorld::IsNightTable(Boss, TEXT("wasteland"));
	const int32 Want = bNight ? S.RouteIndex : 0;
	const FVector Goal = D->GroundPoint(WRLand::Vec2((*Route)[Want % Route->Num()]));
	const FVector To = Goal - Boss->GetActorLocation();
	if (To.Size2D() < 300.f)
	{
		if (bNight) S.RouteIndex = (S.RouteIndex + 1) % Route->Num();
		return !bNight;   // at rest on the VbGlass Sea by day
	}
	Boss->SpeedBoost = VbT(Boss, TEXT("walkSpeed"), 300.f) / FMath::Max(1.f, Boss->GetDef() ? Boss->GetDef()->Speed : 300.f);
	Boss->AddMovementInput(To.GetSafeNormal2D(), 1.f);
	Boss->SetActorRotation(FMath::RInterpConstantTo(Boss->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 90.f));
	Boss->SetHome(Boss->GetActorLocation());
	return true;
}

bool FWRVoidBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsVoidBoss(Boss)) return true;
	const FVbState* S = VbStates().Find(Boss);
	if (!S) return true;
	// Forced moves never come out of the machine's own choice.
	static const TSet<FName> Forced = { TEXT("reform_slam"), TEXT("hollow") };
	if (Forced.Contains(Attack.Id)) return false;
	if (Boss->CreatureId == TEXT("unravelled") && S->UnravelStage != 0) return false;
	if (Boss->CreatureId == TEXT("mother_of_rifts") && Attack.Id == TEXT("open_rift"))
	{
		int32 Open = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : S->Brood) Open += C.IsValid() && C->IsAlive();
		return Open < int32(VbT(Boss, TEXT("broodMax"), 4.f));
	}
	if (Boss->CreatureId == TEXT("starving_one") && VbNow(Boss) < S->EatingUntil) return false;
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		if (S->bForceBeam) return Attack.Id == TEXT("rift_beam") || Attack.Id == TEXT("twin_beam");
		if (VbNow(Boss) < S->ShellUntil && S->ShellHp > 0.f) return Attack.Id != TEXT("lamprey_lunge");
	}
	return true;
}

void FWRVoidBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsVoidBoss(Boss)) return;
	FVbState& S = VbSt(Boss);
	UWRVoidBody* Body = UWRVoidBody::Get(Boss);
	if (Attack.bGrab && Body) { Body->Set(WRVoidFlag::Gape, true); Body->GapeUntil = VbNow(Boss) + Attack.Windup + 0.6f; }
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		if (Attack.Id == TEXT("rift_beam") || Attack.Id == TEXT("twin_beam")) S.bForceBeam = false;
		if (Attack.Id == TEXT("rift_dive") && Target)
		{
			// It leaps off its piece into the black; 1.2 s before it comes up, a lensing ring opens beside your piece and
			// rubble rises round its edge (the tell), with a hold of up to 0.4 s.
			S.DiveAt = Target->GetActorLocation() - FVector(0, 0, Target->GetSimpleCollisionHalfHeight());
			S.DiveRingAt = VbNow(Boss) + FMath::Max(0.f, Attack.Windup - 1.2f);
			WRFx::Broadcast(Boss, TEXT("void.rift.plunge"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("void.rift.plunge"));
			Boss->SetActorHiddenInGame(true);
			Boss->SetActorEnableCollision(false);
		}
		if (Attack.Id == TEXT("inhale")) WRFx::BroadcastOn(Boss, TEXT("void.cast"), TEXT("head"), 6.f, TEXT("void.blackhole"), Attack.Windup + 3.f);
	}
	if (Boss->CreatureId == TEXT("echo") && Attack.Id == TEXT("mimic"))
	{
		// The tail fan: your ability's colour and its sound reversed in the plumes (the 1.0 s tell).
		WRFx::BroadcastOn(Boss, TEXT("void.nowhere"), TEXT("spine"), 2.f, TEXT("void.echo.reverse"), Attack.Windup);
	}
}

bool FWRVoidBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsVoidBoss(Boss)) return false;
	FVbState& S = VbSt(Boss);
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	const FName Id = Attack.Id;
	if (Id == TEXT("open_rift"))
	{
		// A pen's tear widens with a shriek: a brood rift opens there.
		int32 Pick = INDEX_NONE;
		for (int32 i = 0; i < S.Pens.Num(); ++i) if (!S.Brood[i].IsValid() || !S.Brood[i]->IsAlive()) { Pick = i; if (FMath::RandBool()) break; }
		if (Pick != INDEX_NONE)
		{
			if (AWRCreature* R = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("brood_rift"), S.Pens[Pick] + FVector(0, 0, 170.f), FRotator::ZeroRotator))
			{
				S.Brood[Pick] = R;
				S.BroodNext[Pick] = VbNow(Boss) + 2.0;
				S.bHadBrood = true;
				WRFx::Broadcast(Boss, TEXT("void.rift.burst"), S.Pens[Pick], FRotator::ZeroRotator, 1.4f, TEXT("void.rift.open"));
			}
		}
		return true;
	}
	if (Id == TEXT("bell_toll"))
	{
		// The bell lifted overhead rings a 15 m wave: one buff stripped, 30 stamina (guarding halves it).
		WRFx::Broadcast(Boss, TEXT("void.hush.scream"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.5f, TEXT("void.bell.toll"));
		for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > Attack.Range) continue;
			if (WRStorm::TakeOneBuff(*It).IsNone()) WRVoid::TakeBrew(*It);
			const float Drain = 30.f * (It->HasTag(WRTags::State_Blocking) ? 0.5f : 1.f);
			if (UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent()) if (const UWRAttributeSet* A = It->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - Drain));
		}
		return true;
	}
	if (Id == TEXT("hunger_howl"))
	{
		WRFx::Broadcast(Boss, TEXT("void.hush.scream"), Boss->GetActorLocation(), FRotator::ZeroRotator, 1.8f, TEXT("void.hunger.howl"));
		for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > Attack.Range) continue;
			if (UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent()) if (const UWRAttributeSet* A = It->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - 30.f));
		}
		return true;
	}
	if (Id == TEXT("mimic"))
	{
		// Vary: three different path abilities in a row and its Mimic fails (the plumes spark and short): stunned 2 s.
		const int32 N = S.Heard.Num();
		const bool bVaried = N >= 3 && (S.Heard[N - 1].PathId != S.Heard[N - 2].PathId || S.Heard[N - 1].Element != S.Heard[N - 2].Element)
			&& (S.Heard[N - 2].PathId != S.Heard[N - 3].PathId || S.Heard[N - 2].Element != S.Heard[N - 3].Element)
			&& (S.Heard[N - 1].PathId != S.Heard[N - 3].PathId || S.Heard[N - 1].Element != S.Heard[N - 3].Element);
		if (bVaried || N == 0)
		{
			S.FailUntil = VbNow(Boss) + VbT(Boss, TEXT("failStun"), 2.f);
			if (Boss->Status) Boss->Status->Stagger(VbT(Boss, TEXT("failStun"), 2.f));
			WRFx::Broadcast(Boss, TEXT("storm.discharge"), Boss->GetActorLocation(), FRotator::ZeroRotator, 1.2f, TEXT("void.echo.short"));
			S.Heard.Reset();
			return true;
		}
		// Its own version from its beak: never homing, nothing inside 4 m, max 1,600 cm/s, x0.8.
		const FWRHit Last = S.Heard.Last();
		if (!Target || FVector::Dist(Target->GetActorLocation(), Boss->GetActorLocation()) < VbT(Boss, TEXT("mimicMin"), 400.f)) return true;
		FWRHit Hit = Last;
		Hit.Instigator = Boss;
		Hit.Damage = Last.Damage * VbT(Boss, TEXT("mimicShare"), 0.8f);
		Hit.PathId = NAME_None;
		Hit.bProjectile = true;
		Hit.bSpell = true;
		const FVector From = Boss->MouthLocation();
		// Where you stood when its wind-up began, never where you are now (cookpass-combat 3.12).
		const FVector Aim = Boss->AttackPoint + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
		FActorSpawnParameters Params;
		Params.Owner = Boss;
		Params.Instigator = Boss;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* P = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (Aim - From).Rotation(), Params))
		{
			P->Init(Hit, FMath::Min(VbT(Boss, TEXT("mimicSpeed"), 1600.f), WRTune(TEXT("creatureAI.maxProjectileSpeed"), 1000.f)), Last.Element == WRTags::Element_Fire ? FLinearColor(1.f, 0.45f, 0.05f) : Last.Element == WRTags::Element_Frost ? FLinearColor(0.5f, 0.85f, 1.f) : VbDeep);
		}
		return true;
	}
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld());
		if (Id == TEXT("rift_beam") || Id == TEXT("twin_beam"))
		{
			// It rears; the chest hole's rim brightens; a beam 40 m long sweeps the floor (the twin beam stops 0.5 s mid-sweep).
			const bool bTwin = Id == TEXT("twin_beam");
			const float Sweep = VbT(Boss, bTwin ? TEXT("twinSweep") : TEXT("beamSweep"), bTwin ? 200.f : 100.f);
			const float Rate = VbT(Boss, bTwin ? TEXT("twinRate") : TEXT("beamRate"), bTwin ? 35.f : 50.f);
			const float Yaw0 = Boss->GetActorRotation().Yaw - Sweep * 0.5f;
			if (Stage) Stage->StartBeam(Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 0.6f, Yaw0, Sweep, Rate, bTwin ? Sweep * 0.5f / Rate : -1.f, bTwin ? VbT(Boss, TEXT("twinStop"), 0.5f) : 0.f, Attack.Range);
			Boss->StrikeEnds = VbNow(Boss) + Sweep / FMath::Max(1.f, Rate) + (bTwin ? VbT(Boss, TEXT("twinStop"), 0.5f) : 0.f);
			return true;
		}
		if (Id == TEXT("inhale"))
		{
			Boss->StrikeEnds = VbNow(Boss) + 3.f;   // the pull itself; the snap is EndStrike's
			return true;
		}
		if (Id == TEXT("rift_dive"))
		{
			// Up under your piece: 100 and a launch within 5 m (a warded piece takes only the ward).
			const FVector At = S.DiveAt;
			Boss->SetActorLocation(At + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
			Boss->SetActorHiddenInGame(false);
			Boss->SetActorEnableCollision(true);
			WRFx::Broadcast(Boss, TEXT("void.rift.burst"), At, FRotator::ZeroRotator, 3.f, TEXT("void.rift.burst"));
			for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), At) > Attack.Size) continue;
				VbHurt(Boss, *It, Attack.Damage, 5, false, At);
				It->LaunchCharacter(FVector(0, 0, 900.f) + (It->GetActorLocation() - At).GetSafeNormal2D() * 400.f, true, true);
			}
			return true;
		}
		if (Id == TEXT("hollow"))
		{
			// It folds its hooks and draws the void round itself: light bends round its body (the shell: 1,000).
			S.ShellHp = VbT(Boss, TEXT("shellHealth"), 1000.f);
			S.ShellUntil = VbNow(Boss) + VbT(Boss, TEXT("shellSeconds"), 20.f);
			Boss->ShellFraction = 1.f;
			WRFx::BroadcastOn(Boss, TEXT("void.nowhere"), TEXT("spine"), 6.f, TEXT("void.nowhere"), VbT(Boss, TEXT("shellSeconds"), 20.f));
			return true;
		}
	}
	return false;
}

bool FWRVoidBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsVoidBoss(Boss) || Boss->CreatureId != TEXT("vorhollow")) return false;
	AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld());
	if (Attack.Id == TEXT("inhale"))
	{
		// The pull is the tell: dust, pebbles and rubble stream toward the mouth; 350 cm/s in a 120 deg cone to 25 m; a
		// voice pillar between you and the mouth stops it on you. The furthest tame is pulled first (it braces at the edge).
		const FVector Mouth = Boss->MouthLocation();
		const FVector Fwd = Boss->GetActorForwardVector().GetSafeNormal2D();
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Boss->IsHostileTo(*It)) continue;
			const FVector To = Mouth - It->GetActorLocation();
			if (To.Size2D() > Attack.Range || FVector::DotProduct(-To.GetSafeNormal2D(), Fwd) < FMath::Cos(FMath::DegreesToRadians(VbT(Boss, TEXT("inhaleArc"), 60.f)))) continue;
			if (Stage && Stage->PillarBetween(It->GetActorLocation(), Mouth)) continue;
			It->LaunchCharacter(To.GetSafeNormal2D() * VbT(Boss, TEXT("inhalePull"), 350.f), true, false);
		}
		return true;
	}
	if (Attack.Id == TEXT("rift_beam") || Attack.Id == TEXT("twin_beam"))
	{
		// 25 a quarter second where the beam passes (a roll's i-frames cover one tick; a pillar's shadow covers you).
		float Yaw = 0.f;
		if (!Stage || !Stage->BeamNow(Yaw)) return true;
		const FVector O = Stage->BeamOrigin;
		const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Boss->IsHostileTo(*It)) continue;
			const FVector P = It->GetActorLocation();
			const FVector Q = FMath::ClosestPointOnSegment(P, O, O + Dir * Attack.Range);
			if (FVector::Dist2D(P, Q) > Attack.Size + It->GetSimpleCollisionRadius() || Stage->PillarBetween(P, O)) continue;
			if (Boss->StrikeHits.Contains(*It)) continue;
			Boss->StrikeHits.Add(*It);
			VbHurt(Boss, *It, Attack.Damage, 2, true, Q, 10.f);
			TWeakObjectPtr<AWRBoss> WB(Boss);
			TWeakObjectPtr<AWRCharacterBase> WV(*It);
			FTimerHandle H;
			Boss->GetWorldTimerManager().SetTimer(H, [WB, WV]() { if (WB.IsValid() && WV.IsValid()) WB->StrikeHits.Remove(WV); }, 0.25f, false);
		}
		return true;
	}
	return false;
}

void FWRVoidBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsVoidBoss(Boss)) return;
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld());
		if (Attack.Id == TEXT("inhale"))
		{
			// The snap: 80 to everything within 3 m of the mouth; a tame takes 150.
			const FVector Mouth = Boss->MouthLocation();
			WRFx::Broadcast(Boss, TEXT("void.crush"), Mouth, FRotator::ZeroRotator, 2.f, TEXT("void.maw.close"));
			for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !Boss->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), Mouth) > 300.f + It->GetSimpleCollisionRadius()) continue;
				VbHurt(Boss, *It, Cast<AWRCreature>(*It) && Cast<AWRCreature>(*It)->IsTamed() ? 150.f : Attack.Damage, 5, false, Mouth);
			}
		}
		if ((Attack.Id == TEXT("rift_beam") || Attack.Id == TEXT("twin_beam")) && Stage) Stage->EndBeam();
		if (Attack.Id == TEXT("gnashing") && Stage)
		{
			// The bite lands: the four chains snap one after another and the island splits along the beam's cracks.
			Stage->Split(true);
		}
	}
	if (Boss->CreatureId == TEXT("unravelled") && Attack.Id == TEXT("reform_slam"))
	{
		VbSt(Boss).UnravelStage = 0;
	}
}

void FWRVoidBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (!IsVoidBoss(Boss)) return;
	Hit.Element = WRTags::Element_Void;
	if (Boss->CreatureId == TEXT("starving_one"))
	{
		const FVbState* S = VbStates().Find(Boss);
		if (S) Hit.Damage *= 1.f + VbT(Boss, TEXT("damagePer"), 0.1f) * S->Stacks;
	}
}

void FWRVoidBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsVoidBoss(Boss) || !Victim || (Result != EWRHitResult::Hit && Result != EWRHitResult::GuardBroken)) return;
	FVbState& S = VbSt(Boss);
	if (Boss->CreatureId == TEXT("starving_one") && Result == EWRHitResult::Hit)
	{
		// Every blow that lands eats one of your buffs; it grows on it.
		bool bAte = !WRStorm::TakeOneBuff(Victim).IsNone() || WRVoid::TakeBrew(Victim);
		if (!bAte && Victim->HasTag(WRVoidPath::Status_Sacrifice)) { if (Victim->Status) Victim->Status->ClearStatus(WRVoidPath::Status_Sacrifice); bAte = true; }
		if (bAte && S.Stacks < int32(VbT(Boss, TEXT("maxStacks"), 5.f)))
		{
			++S.Stacks;
			Boss->SpeedBoost = 1.f + VbT(Boss, TEXT("speedPer"), 0.05f) * S.Stacks;
			if (UWRVoidBody* B = UWRVoidBody::Get(Boss)) B->Stacks = uint8(S.Stacks);
			WRFx::BroadcastOn(Boss, TEXT("void.feed"), TEXT("spine"), 2.f, TEXT("void.consume"));
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(Victim->GetController())) PC->ClientNotify(TEXT("It ate what you'd taken: it grows"));
		}
	}
	if (Boss->CreatureId == TEXT("vorhollow") && AttackId == TEXT("lamprey_lunge") && Result == EWRHitResult::Hit)
	{
		// Swallowed: 15% of your max HP into the band, and you drop out through the chest hole behind it, knocked down.
		const UWRAttributeSet* A = Victim->GetAttributes();
		if (A) WRVoid::LoseMaxHealth(Victim, A->GetMaxHealth() * VbT(Boss, TEXT("swallowShare"), 0.15f), Boss);
		const FVector Behind = Boss->GetActorLocation() - Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 300.f);
		TWeakObjectPtr<AWRCharacterBase> WV(Victim);
		FTimerHandle H;
		Boss->GetWorldTimerManager().SetTimer(H, [WV, Behind]()
		{
			if (!WV.IsValid() || !WV->IsAlive()) return;
			WV->TeleportTo(Behind, WV->GetActorRotation(), false, true);
			if (WV->Status) WV->Status->Stagger(1.5f);
			WRFx::Broadcast(WV.Get(), TEXT("void.crush"), Behind, FRotator::ZeroRotator, 1.5f, TEXT("void.consume"));
		}, 1.6f, false);
		if (Victim->Status) Victim->Status->Stagger(1.6f);
	}
	if (Boss->CreatureId == TEXT("mother_of_rifts") && AttackId == TEXT("grab") && Victim->Status) Victim->Status->Stagger(1.4f);
}

float FWRVoidBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsVoidBoss(Boss)) return Damage;
	FVbState& S = VbSt(Boss);
	if (Hit.Element == WRTags::Element_Void) Damage *= VbT(Boss, TEXT("voidTaken"), 0.75f);
	if (Boss->CreatureId == TEXT("unravelled"))
	{
		// The pieces part round a blow (x0.3); the core alone in the air takes x3.
		if (S.UnravelStage == 1 || S.UnravelStage == 3) return 0.f;
		return Damage * (S.UnravelStage == 2 ? VbT(Boss, TEXT("coreTaken"), 3.f) : VbT(Boss, TEXT("bodyTaken"), 0.3f));
	}
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		if (Boss->IsHidden()) return 0.f;   // under the stage, mid-dive
		if (S.ShellHp > 0.f && VbNow(Boss) < S.ShellUntil)
		{
			// The Hollow: x0.2 through the shell; void heals the shell (x0.5); the rest wears it down.
			if (Hit.Element == WRTags::Element_Void) S.ShellHp += Damage * 0.5f;
			else S.ShellHp -= Damage;
			Boss->ShellFraction = FMath::Clamp(S.ShellHp / FMath::Max(1.f, VbT(Boss, TEXT("shellHealth"), 1000.f)), 0.f, 1.f);
			if (S.ShellHp <= 0.f)
			{
				S.ShellHp = 0.f;
				if (Boss->Status) Boss->Status->Stagger(VbT(Boss, TEXT("shellBreakStagger"), 3.f));
				Boss->StanceOpenUntil = VbNow(Boss) + VbT(Boss, TEXT("shellBreakStagger"), 3.f);
				WRFx::Broadcast(Boss, TEXT("void.crush"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("void.maw.close"));
			}
			return Damage * VbT(Boss, TEXT("shellTaken"), 0.2f);
		}
		if (Boss->Phase >= 3) Damage *= VbT(Boss, TEXT("bodyShareP3"), 0.75f);
	}
	return Damage;
}

void FWRVoidBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsVoidBoss(Boss)) return;
	FVbState& S = VbSt(Boss);
	if (Boss->CreatureId == TEXT("echo") && !Hit.PathId.IsNone()) NoteAbility(Boss, Hit);
	if (Boss->CreatureId == TEXT("unravelled") && S.UnravelStage == 0)
	{
		// After 300 damage or a heavy blow it unravels: the pieces burst into a whirling column and travel 8-15 m.
		S.SinceReform += Damage;
		if (S.SinceReform >= VbT(Boss, TEXT("unravelAfter"), 300.f) || Hit.Impact >= 4 || Hit.bHeavy)
		{
			S.SinceReform = 0.f;
			S.UnravelStage = 1;
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			S.TravelFrom = Boss->GetActorLocation();
			FVector To = Boss->GetHome() + Dir * FMath::FRandRange(VbT(Boss, TEXT("travelMin"), 800.f), VbT(Boss, TEXT("travelMax"), 1500.f)) * 0.8f;
			To.Z = WRLand::GroundZAt(To, S.TravelFrom.Z - Boss->GetSimpleCollisionHalfHeight()) + Boss->GetSimpleCollisionHalfHeight();
			S.TravelTo = To;
			S.TravelStart = VbNow(Boss);
			S.StageUntil = VbNow(Boss) + FMath::Max(0.6f, FVector::Dist(S.TravelFrom, To) / 1200.f);
			if (UWRVoidBody* B = UWRVoidBody::Get(Boss)) B->Set(WRVoidFlag::Drifting, true);
			WRFx::Broadcast(Boss, TEXT("void.collapse"), S.TravelFrom, FRotator::ZeroRotator, 0.8f, TEXT("void.unravelled.burst"));
		}
	}
}

float FWRVoidBoss::StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance)
{
	const AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!IsVoidBoss(Boss)) return 1.f;
	const FVbState* S = VbStates().Find(Boss);
	if (!S) return 1.f;
	const double T = VbNow(Boss);
	if (Boss->CreatureId == TEXT("unravelled") && S->UnravelStage == 2) return VbT(Boss, TEXT("coreStance"), 4.f);
	if (Boss->CreatureId == TEXT("mother_of_rifts") && T < S->StrainUntil) return VbT(Boss, TEXT("strainStance"), 2.f);
	if (Boss->CreatureId == TEXT("starving_one") && T < S->EatingUntil) return VbT(Boss, TEXT("eatStance"), 2.f);
	if (Boss->CreatureId == TEXT("echo") && T < S->FailUntil) return VbT(Boss, TEXT("failStance"), 2.f);
	return 1.f;
}

void FWRVoidBoss::NoteAbility(AWRBoss* Boss, const FWRHit& Hit)
{
	FVbState& S = VbSt(Boss);
	S.Heard.Add(Hit);
	while (S.Heard.Num() > 3) S.Heard.RemoveAt(0);
}

void FWRVoidBoss::HelperDied(AWRCreature* Helper)
{
	if (!Helper || Helper->CreatureId != TEXT("brood_rift")) return;
	for (TPair<TWeakObjectPtr<const AWRBoss>, FVbState>& KV : VbStates())
	{
		AWRBoss* Boss = const_cast<AWRBoss*>(KV.Key.Get());
		if (!Boss || Boss->CreatureId != TEXT("mother_of_rifts")) continue;
		FVbState& S = KV.Value;
		int32 Open = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : S.Brood) Open += C.IsValid() && C->IsAlive() && C.Get() != Helper;
		WRFx::Broadcast(Helper, TEXT("void.rift.plunge"), Helper->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("void.rift.plunge"));
		if (Open == 0 && S.bHadBrood)
		{
			// Every brood rift closed: the strain stuns her 4 s (stance x2 taken): the punish window.
			S.bHadBrood = false;
			S.StrainUntil = VbNow(Boss) + VbT(Boss, TEXT("strainStun"), 4.f);
			if (Boss->Status) Boss->Status->Stagger(VbT(Boss, TEXT("strainStun"), 4.f));
			WRFx::Broadcast(Boss, TEXT("void.crush"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("void.mother.strain"));
		}
	}
}

void FWRVoidBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsVoidBoss(Boss) || Boss->CreatureId != TEXT("vorhollow")) return;
	if (NewPhase >= 3)
	{
		// Inside: it climbs onto the Stage and opens its chest wide; a pull covers the arena toward the hole.
		VbSt(Boss).bGulletOpen = true;
		if (AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld())) Stage->OpenGullet(true);
	}
}

void FWRVoidBoss::Died(AWRBoss* Boss)
{
	if (!IsVoidBoss(Boss)) return;
	FVbState& S = VbSt(Boss);
	for (const TWeakObjectPtr<AWRCreature>& C : S.Brood) if (C.IsValid()) C->Destroy();
	// Maren's quests: the Choir's road counts the first two stations (each player near the kill).
	if (Boss->CreatureId == TEXT("unravelled") || Boss->CreatureId == TEXT("mother_of_rifts"))
	{
		for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) < 6000.f) WRVoidWorld::QuestEvent(*It, TEXT("choir_road"));
		}
	}
	if (Boss->CreatureId == TEXT("null_warden"))
	{
		// His key drops into the lock socket (it was built to): the bar slides, the counterweights fall, the gate rises.
		if (AWRNullGate* G = AWRNullGate::Find(Boss->GetWorld())) G->SetOpen(true);
	}
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		if (AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld())) { Stage->OpenGullet(false); Stage->EndBeam(); }
	}
}

bool FWRVoidBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsVoidBoss(Boss) || !Boss->HasAuthority()) return false;
	FVbState& S = VbSt(Boss);
	const double T = VbNow(Boss);
	if (Boss->CreatureId == TEXT("unravelled") && S.UnravelStage != 0)
	{
		UWRVoidBody* B = UWRVoidBody::Get(Boss);
		if (S.UnravelStage == 1)
		{
			// The whirling column travels (10 a touch within 3 m).
			const float A = FMath::Clamp(float((T - S.TravelStart) / FMath::Max(0.1, S.StageUntil - S.TravelStart)), 0.f, 1.f);
			Boss->SetActorLocation(FMath::Lerp(S.TravelFrom, S.TravelTo, FMath::InterpEaseInOut(0.f, 1.f, A, 2.f)), false, nullptr, ETeleportType::TeleportPhysics);
			for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation()) < VbT(Boss, TEXT("whirlRadius"), 300.f) && !Boss->StrikeHits.Contains(*It))
				{
					Boss->StrikeHits.Add(*It);
					VbHurt(Boss, *It, VbT(Boss, TEXT("whirlDamage"), 10.f), 1, true, Boss->GetActorLocation());
				}
			}
			if (T >= S.StageUntil)
			{
				// It reforms over 1.2 s: for the first 0.6 the core hangs alone, exposed.
				S.UnravelStage = 2;
				S.StageUntil = T + VbT(Boss, TEXT("coreOpen"), 0.6f);
				Boss->StrikeHits.Reset();
				if (B) { B->Set(WRVoidFlag::Drifting, false); B->Set(WRVoidFlag::Gape, true); B->GapeUntil = S.StageUntil; }
				WRSound::PlayAt(Boss, TEXT("void.unravelled.core"), Boss->GetActorLocation());
			}
			return true;
		}
		if (S.UnravelStage == 2 && T >= S.StageUntil)
		{
			// Then the pieces swirl inward, loud and visible, and snap shut: the Reform slam.
			S.UnravelStage = 3;
			S.StageUntil = T + VbT(Boss, TEXT("reform"), 1.2f) - VbT(Boss, TEXT("coreOpen"), 0.6f);
			WRFx::Broadcast(Boss, TEXT("void.collapse"), Boss->GetActorLocation(), FRotator::ZeroRotator, 0.5f, TEXT("void.unravelled.swirl"));
		}
		if (S.UnravelStage == 3 && T >= S.StageUntil)
		{
			if (FWRBossAttack* Slam = FindAttack(Boss, TEXT("reform_slam")))
			{
				AWRCharacterBase* Near = VbNearestPlayer(Boss, 4000.f);
				if (Near) { Boss->StartAttack(*Slam, Near); S.UnravelStage = 4; return true; }
			}
			S.UnravelStage = 0;
		}
		return S.UnravelStage != 0 && S.UnravelStage != 4;
	}
	if (Boss->CreatureId == TEXT("mother_of_rifts"))
	{
		// Each open brood rift spits a Gnawer swarm (6) or a Hollowfang every 15 s.
		for (int32 i = 0; i < S.Brood.Num(); ++i)
		{
			AWRCreature* R = S.Brood[i].Get();
			if (!R || !R->IsAlive() || T < S.BroodNext[i]) continue;
			S.BroodNext[i] = T + VbT(Boss, TEXT("broodEvery"), 15.f);
			const bool bGnawers = FMath::RandBool();
			const int32 N = bGnawers ? int32(VbT(Boss, TEXT("broodGnawers"), 6.f)) : 1;
			AActor* Prey = VbNearestPlayer(Boss, 6000.f);
			for (int32 k = 0; k < N; ++k)
			{
				const FVector At = R->GetActorLocation() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f);
				if (AWRCreature* C = AWRCreature::SpawnCreature(Boss->GetWorld(), bGnawers ? FName(TEXT("gnawer")) : FName(TEXT("hollowfang")), At, FRotator::ZeroRotator))
				{
					C->SetHome(Boss->GetHome());
					if (Prey) C->SetAggroTarget(Prey);
				}
			}
			WRFx::Broadcast(Boss, TEXT("void.rift.burst"), R->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("void.rift.burst"));
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("starving_one"))
	{
		// It smells food on the ground within 15 m and can't refuse it: it runs to eat (2.5 s), each one a stack off.
		if (T < S.EatingUntil) return true;
		if (!S.Bait.IsValid() && S.Stacks >= 0)
		{
			for (TActorIterator<AWRLootBag> It(Boss->GetWorld()); It; ++It)
			{
				if (FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > VbT(Boss, TEXT("smell"), 1500.f)) continue;
				const bool bFood = It->Items.ContainsByPredicate([](const FWRItemStack& X) { const FWRItemDef* D = FWRItemRegistry::Get().Find(X.ItemId); return D && D->Category == TEXT("food") && X.Count > 0; });
				if (bFood) { S.Bait = *It; break; }
			}
		}
		if (AWRLootBag* Bag = S.Bait.Get())
		{
			const FVector To = Bag->GetActorLocation() - Boss->GetActorLocation();
			if (To.Size2D() > 260.f)
			{
				Boss->AddMovementInput(To.GetSafeNormal2D(), 1.f);
				Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
				return true;
			}
			for (int32 i = 0; i < Bag->Items.Num(); ++i)
			{
				const FWRItemDef* D = FWRItemRegistry::Get().Find(Bag->Items[i].ItemId);
				if (D && D->Category == TEXT("food") && Bag->Items[i].Count > 0)
				{
					if (--Bag->Items[i].Count <= 0) Bag->Items.RemoveAt(i);
					break;
				}
			}
			if (Bag->Items.Num() == 0) Bag->Destroy();
			S.Bait = nullptr;
			S.EatingUntil = T + VbT(Boss, TEXT("eatSeconds"), 2.5f);
			S.Stacks = FMath::Max(0, S.Stacks - 1);
			Boss->SpeedBoost = 1.f + VbT(Boss, TEXT("speedPer"), 0.05f) * S.Stacks;
			if (UWRVoidBody* B = UWRVoidBody::Get(Boss)) B->Stacks = uint8(S.Stacks);
			WRFx::BroadcastOn(Boss, TEXT("void.consume"), TEXT("head"), 1.5f, TEXT("void.starving.eat"), VbT(Boss, TEXT("eatSeconds"), 2.5f));
			return true;
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("null_warden"))
	{
		// Projectiles entering his field drop dead.
		const float Field = VbT(Boss, TEXT("field"), 1200.f);
		for (TActorIterator<AWRProjectile> It(Boss->GetWorld()); It; ++It)
		{
			const AWRCharacterBase* By = Cast<AWRCharacterBase>(It->GetInstigator());
			if (By && By != Boss && FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) < Field)
			{
				WRFx::Broadcast(Boss, TEXT("dust.cloud"), It->GetActorLocation(), FRotator::ZeroRotator, 0.4f, TEXT("void.fizzle"));
				It->Destroy();
			}
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("vorhollow"))
	{
		AWRVorhollowStage* Stage = AWRVorhollowStage::Find(Boss->GetWorld());
		// The Hollow at 80%, 60% and 40% (forced); a ringing voice pillar within 12 m wears the shell 150 a second.
		const float Frac = Boss->GetAttributes() ? Boss->GetAttributes()->GetHealth() / FMath::Max(1.f, Boss->GetAttributes()->GetMaxHealth()) : 1.f;
		static const float Marks[3] = { 0.8f, 0.6f, 0.4f };
		if (S.ShellsDone < 3 && Frac < Marks[S.ShellsDone] && Boss->Current == INDEX_NONE)
		{
			if (FWRBossAttack* H = FindAttack(Boss, TEXT("hollow")))
			{
				if (AWRCharacterBase* Near = VbNearestPlayer(Boss, 8000.f)) { ++S.ShellsDone; Boss->StartAttack(*H, Near); }
			}
		}
		if (S.ShellHp > 0.f)
		{
			if (T >= S.ShellUntil)
			{
				// Unbroken after 20 s it drops by itself, and its next move is a Rift beam.
				S.ShellHp = 0.f;
				Boss->ShellFraction = 0.f;
				S.bForceBeam = true;
			}
			else if (Stage)
			{
				for (int32 i = 0; i < 6; ++i)
				{
					if (Stage->PillarRinging(i) && FVector::Dist(Stage->PillarAt(i), Boss->GetActorLocation()) < VbT(Boss, TEXT("pillarReach"), 1200.f) + Boss->GetSimpleCollisionRadius())
					{
						S.ShellHp -= VbT(Boss, TEXT("pillarDps"), 150.f) * DeltaSeconds;
					}
				}
				Boss->ShellFraction = FMath::Clamp(S.ShellHp / FMath::Max(1.f, VbT(Boss, TEXT("shellHealth"), 1000.f)), 0.f, 1.f);
				if (S.ShellHp <= 0.f && Boss->Status) { S.ShellHp = 0.f; Boss->Status->Stagger(VbT(Boss, TEXT("shellBreakStagger"), 3.f)); }
			}
		}
		// The dive's tell: the lensing ring opens beside your piece and rubble rises round its edge.
		if (S.DiveRingAt > 0.0 && T >= S.DiveRingAt)
		{
			S.DiveRingAt = 0.0;
			WRFx::Broadcast(Boss, TEXT("void.blink.ripple"), S.DiveAt, FRotator::ZeroRotator, 4.f, TEXT("void.rift.grind"), 1.2f);
		}
		return false;
	}
	return false;
}

FWRBossAttack* FWRVoidBoss::FindAttack(AWRBoss* Boss, FName Id)
{
	return Boss->Attacks.FindByPredicate([Id](const FWRBossAttack& A) { return A.Id == Id; });
}

bool FWRVoidBoss::InNullField(const UWorld* World, const FVector& At)
{
	if (!World) return false;
	for (TActorIterator<AWRBoss> It(World); It; ++It)
	{
		if (It->CreatureId == TEXT("null_warden") && It->IsAlive() && FVector::Dist(It->GetActorLocation(), At) < WRTune(TEXT("bosses.null_warden.field"), 1200.f)) return true;
	}
	return false;
}

// ================================================================================================ the arenas

bool FWRVoidBoss::HeavySwing(const AWRCharacterBase* Who)
{
	const UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !Who->HasTag(WRTags::State_Attacking)) return false;
	if (Who->HasTag(WRTags::State_Charging)) return true;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability) continue;
		const FGameplayTagContainer& Tags = Spec.Ability->GetAssetTags();
		if (Tags.HasTag(WRTags::Ability_Heavy) || (Tags.HasTag(WRTags::Ability_Path) && Tags.HasTag(WRTags::Ability_Melee))) return true;
	}
	return false;
}

void FWRVoidBoss::PlaceArenas(AWRLand* Land)
{
	// Anchor salt answers here: brood rifts close, island pieces hold (10 s).
	static bool bBound = false;
	if (!bBound)
	{
		bBound = true;
		WRVoidWorld::OnSalted().AddLambda([](const FVector& At, float Radius, AWRCharacterBase* By)
		{
			UWorld* W = By ? By->GetWorld() : nullptr;
			if (!W) return;
			for (TActorIterator<AWRCreature> It(W); It; ++It)
			{
				if (It->CreatureId == TEXT("brood_rift") && It->IsAlive() && FVector::Dist(It->GetActorLocation(), At) < Radius + 200.f) VbHurt(By, *It, 99999.f, 3, true, At);
			}
			if (AWRVorhollowStage* Stage = AWRVorhollowStage::Find(W)) Stage->WardAt(At, WRTune(TEXT("bosses.vorhollow.saltWard"), 10.f));
		});
	}
	const FWRLandDef* D = Land ? Land->Def() : nullptr;
	UWorld* World = Land ? Land->GetWorld() : nullptr;
	const TSharedPtr<FJsonObject> Bosses = D ? D->Section(TEXT("bosses")) : nullptr;
	if (!World || !Bosses.IsValid() || World->GetNetMode() == NM_Client) return;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Bosses->Values)
	{
		if (KV.Key.StartsWith(TEXT("_"))) continue;
		const TSharedPtr<FJsonObject> R = KV.Value->AsObject();
		const FVector2D At = VbV2(R, TEXT("at"));
		FVector G = D->GroundPoint(At);
		if (R->HasField(TEXT("lipBelow")))
		{
			// The island hangs at the rift's centre below the lip (the stage places itself; the boss sits on its dais).
			const FVector2D Lip = VbV2(R, TEXT("lip"));
			const float LipZ = D->Origin.Z + D->Height(Lip) * 100.f;
			G = FVector(G.X, G.Y, LipZ - R->GetNumberField(TEXT("lipBelow")) * 100.f);
			const FTransform ST(G);
			if (AWRVorhollowStage* Stage = World->SpawnActorDeferred<AWRVorhollowStage>(AWRVorhollowStage::StaticClass(), ST, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Stage->LipZ = LipZ;
				Stage->FinishSpawning(ST);
			}
			G.Z += 600.f;   // the dais top
		}
		if (R->HasField(TEXT("gate")))
		{
			const FVector2D GateAt = VbV2(R, TEXT("gate"));
			const FVector GW = D->GroundPoint(GateAt);
			const FVector2D Face = VbV2(R, TEXT("gateFace"));
			const FTransform GT(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Face.Y - GateAt.Y, Face.X - GateAt.X)), 0.f), GW);
			World->SpawnActor<AWRNullGate>(AWRNullGate::StaticClass(), GT);
		}
		const FTransform Where(FRotator(0.f, float(R->HasField(TEXT("yaw")) ? R->GetNumberField(TEXT("yaw")) : 0.0), 0.f), G + FVector(0, 0, 100.f));
		if (AWRCreatureSpawner* Sp = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Sp->CreatureId = FName(*KV.Key);
			Sp->Count = 1;
			Sp->Radius = 1.f;
			Sp->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
			Sp->ActivationRadius = 0.f;
			Sp->bTraceGround = !R->HasField(TEXT("lipBelow"));
			Sp->FinishSpawning(Where);
		}
		UE_LOG(LogWildreach, Log, TEXT("VOID boss %s's arena at %s"), *KV.Key, *G.ToCompactString());
	}
}

// ================================================================================================ the Null Gate

AWRNullGate::AWRNullGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bAlwaysRelevant = true;
}

void AWRNullGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRNullGate, bOpen);
	DOREPLIFETIME(AWRNullGate, ChangedAt);
	DOREPLIFETIME(AWRNullGate, FromOpenness);
}

double AWRNullGate::Now() const { return VbSNow(this); }

AWRNullGate* AWRNullGate::Find(const UWorld* World)
{
	for (TActorIterator<AWRNullGate> It(World); It; ++It) return *It;
	return nullptr;
}

float AWRNullGate::Openness() const
{
	const float Secs = WRTune(TEXT("voidBosses.gate.seconds"), 6.f);
	const float A = FMath::Clamp(float(Now() - ChangedAt) / FMath::Max(0.1f, Secs), 0.f, 1.f);
	const float E = FMath::InterpEaseInOut(0.f, 1.f, A, 2.f);
	return bOpen ? FMath::Lerp(FromOpenness, 1.f, E) : FMath::Lerp(FromOpenness, 0.f, E);
}

void AWRNullGate::SetOpen(bool bWant)
{
	if (!HasAuthority() || bWant == bOpen) return;
	FromOpenness = Openness();
	bOpen = bWant;
	ChangedAt = Now();
	WRSound::PlayAt(this, bWant ? FName(TEXT("void.gate.raise")) : FName(TEXT("void.gate.lower")), GetActorLocation() + FVector(0, 0, 400.f));
	UE_LOG(LogWildreach, Log, TEXT("VOID the Null Gate %s"), bWant ? TEXT("rises") : TEXT("lowers"));
}

void AWRNullGate::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void AWRNullGate::Build()
{
	// Local frame: X through the gate (toward the bridge), Y across it. The gatehouse: two counterweight towers either side
	// of the 8 m opening (their shafts open at slit windows), a roof beam carrying the two pulleys, curtain walls out to the
	// court's sides, the portcullis (a grid of iron bars) hung on two chains from the pulleys down to the counterweights.
	UMaterialInterface* Stone = WRFoldLook::RockMat(this, VbDolo * 0.35f);
	UMaterialInterface* Iron = WRFoldLook::RockMat(this, FLinearColor(0.035f, 0.033f, 0.035f));
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const auto Solid = [&](const TCHAR* Shape, const FVector& Size, const FVector& At, const FRotator& Rot, UMaterialInterface* M, bool bCollide) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = WRFoldLook::Shape(this, RootComponent, Shape, Size, At, Rot, M);
		if (C && bCollide) { C->SetCollisionProfileName(TEXT("BlockAll")); C->SetCanEverAffectNavigation(false); }
		if (C && !bDraw) C->SetVisibility(false);
		return C;
	};
	for (const float Side : { -1.f, 1.f })
	{
		// The counterweight tower: a hollow shaft (four walls), a slit window facing the court, the stone weight inside.
		const FVector Tower(0.f, Side * 650.f, 0.f);
		Solid(TEXT("Cube"), FVector(500.f, 60.f, 1300.f), Tower + FVector(0, -Side * 220.f, 650.f), FRotator::ZeroRotator, Stone, true);
		Solid(TEXT("Cube"), FVector(500.f, 60.f, 1300.f), Tower + FVector(0, Side * 220.f, 650.f), FRotator::ZeroRotator, Stone, true);
		Solid(TEXT("Cube"), FVector(60.f, 500.f, 1300.f), Tower + FVector(220.f, 0, 650.f), FRotator::ZeroRotator, Stone, true);
		Solid(TEXT("Cube"), FVector(60.f, 500.f, 560.f), Tower + FVector(-220.f, 0, 280.f), FRotator::ZeroRotator, Stone, true);
		Solid(TEXT("Cube"), FVector(60.f, 500.f, 560.f), Tower + FVector(-220.f, 0, 1020.f), FRotator::ZeroRotator, Stone, true);   // the slit between 560 and 740
		Solid(TEXT("Cube"), FVector(560.f, 560.f, 60.f), Tower + FVector(0, 0, 1330.f), FRotator::ZeroRotator, Stone, false);
		if (UStaticMeshComponent* W = Solid(TEXT("Cube"), FVector(300.f, 300.f, 260.f), Tower + FVector(0, 0, 900.f), FRotator::ZeroRotator, Stone, false)) Weights.Add(W);
		// The curtain wall out to the court's side.
		Solid(TEXT("Cube"), FVector(160.f, 900.f, 800.f), FVector(0, Side * 1350.f, 400.f), FRotator::ZeroRotator, Stone, true);
		// The pulley in the roof beam over the opening's side.
		if (UStaticMeshComponent* P = Solid(TEXT("Cylinder"), FVector(160.f, 160.f, 40.f), FVector(0, Side * 380.f, 1260.f), FRotator(0.f, 0.f, 90.f), Iron, false)) Pulleys.Add(P);
		ChainEnds.Add(TPair<FVector, FVector>(FVector(0, Side * 380.f, 1180.f), Tower + FVector(0, 0, 1030.f)));
	}
	Solid(TEXT("Cube"), FVector(300.f, 1000.f, 120.f), FVector(0, 0, 1300.f), FRotator::ZeroRotator, Stone, false);   // the roof beam
	// The portcullis: vertical and cross bars, pointed feet; its own frame so it rises whole.
	Portcullis = NewObject<USceneComponent>(this);
	Portcullis->SetupAttachment(RootComponent);
	Portcullis->RegisterComponent();
	for (int32 i = 0; i < 9; ++i)
	{
		const float Y = -380.f + 95.f * i;
		if (UStaticMeshComponent* B = WRFoldLook::Shape(this, Portcullis, TEXT("Cube"), FVector(16.f, 16.f, 760.f), FVector(0, Y, 400.f), FRotator::ZeroRotator, Iron)) B->SetVisibility(bDraw);
		if (UStaticMeshComponent* B = WRFoldLook::Shape(this, Portcullis, TEXT("Cone"), FVector(22.f, 22.f, 40.f), FVector(0, Y, 10.f), FRotator(180.f, 0.f, 0.f), Iron)) B->SetVisibility(bDraw);
	}
	for (int32 j = 0; j < 5; ++j) if (UStaticMeshComponent* B = WRFoldLook::Shape(this, Portcullis, TEXT("Cube"), FVector(20.f, 780.f, 14.f), FVector(0, 0, 120.f + 160.f * j), FRotator::ZeroRotator, Iron)) B->SetVisibility(bDraw);
	Blocker = NewObject<UBoxComponent>(this);
	Blocker->SetupAttachment(Portcullis);
	Blocker->SetBoxExtent(FVector(30.f, 400.f, 400.f));
	Blocker->SetRelativeLocation(FVector(0, 0, 400.f));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->RegisterComponent();
	// The chain links running pulley -> portcullis top and pulley -> weight (repositioned each frame as it moves).
	if (bDraw)
	{
		for (int32 k = 0; k < 2 * 2 * 14; ++k)
		{
			if (UStaticMeshComponent* L = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(14.f, 6.f, 18.f), FVector::ZeroVector, FRotator::ZeroRotator, Iron)) Links.Add(L);
		}
	}
	// The crank winch on the bridge side (-X): a drum on two posts, a crank, the chain running into the wall.
	Solid(TEXT("Cube"), FVector(40.f, 40.f, 160.f), FVector(-400.f, -560.f, 80.f), FRotator::ZeroRotator, Iron, true);
	Solid(TEXT("Cube"), FVector(40.f, 40.f, 160.f), FVector(-400.f, -760.f, 80.f), FRotator::ZeroRotator, Iron, true);
	Solid(TEXT("Cylinder"), FVector(90.f, 90.f, 200.f), FVector(-400.f, -660.f, 140.f), FRotator(0.f, 0.f, 90.f), Iron, false);
	Solid(TEXT("Cube"), FVector(12.f, 12.f, 70.f), FVector(-400.f, -810.f, 160.f), FRotator(30.f, 0.f, 0.f), Iron, false);
	if (bDraw) WRFoldLook::Light(this, RootComponent, FVector(-300.f, 0, 900.f), FLinearColor(1.f, 0.7f, 0.45f), 2500.f, 1500.f);
}

void AWRNullGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The portcullis rises 7 m as the counterweights fall 4 m; the pulleys turn with the chain; all from the clock.
	const float O = Openness();
	if (Portcullis) Portcullis->SetRelativeLocation(FVector(0, 0, 700.f * O));
	for (int32 i = 0; i < Weights.Num(); ++i) if (Weights[i]) { FVector L = Weights[i]->GetRelativeLocation(); L.Z = 900.f - 400.f * O; Weights[i]->SetRelativeLocation(L); }
	for (USceneComponent* P : Pulleys) if (P) P->SetRelativeRotation(FRotator(0.f, 360.f * O * 3.f, 90.f));
	if (Links.Num() >= 56)
	{
		int32 n = 0;
		for (int32 s = 0; s < 2; ++s)
		{
			const FVector Pulley = ChainEnds[s].Key;
			const FVector WeightTop = ChainEnds[s].Value - FVector(0, 0, 400.f * O);
			const FVector GateTop(0, Pulley.Y, 780.f + 700.f * O);
			for (int32 k = 0; k < 14; ++k, ++n) { const FVector P = FMath::Lerp(Pulley, GateTop, (k + 0.5f) / 14.f); Links[n]->SetRelativeLocationAndRotation(P, FRotator(0.f, k % 2 ? 90.f : 0.f, 0.f)); }
			for (int32 k = 0; k < 14; ++k, ++n) { const FVector P = FMath::Lerp(Pulley, WeightTop, (k + 0.5f) / 14.f); Links[n]->SetRelativeLocationAndRotation(P, FRotator(0.f, k % 2 ? 90.f : 0.f, 0.f)); }
		}
	}
	if (!HasAuthority()) return;
	// The crank: held 8 s from the bridge side raises it.
	for (auto It = CrankStarted.CreateIterator(); It; ++It)
	{
		const APawn* P = Cast<APawn>(It->Key.Get());
		if (!P || !CrankNear(P->GetActorLocation())) { It.RemoveCurrent(); continue; }
		if (Now() - It->Value >= WRTune(TEXT("voidBosses.gate.crankSeconds"), 8.f)) { SetOpen(true); It.RemoveCurrent(); }
	}
}

bool AWRNullGate::CrankNear(const FVector& At) const
{
	return FVector::Dist(GetActorTransform().TransformPosition(FVector(-400.f, -660.f, 100.f)), At) < 300.f;
}

void AWRNullGate::Crank(AWRPlayerController* By)
{
	if (!By || !By->GetPawn() || !HasAuthority()) return;
	CrankStarted.FindOrAdd(By->GetPawn()) = Now();
	WRSound::PlayAt(this, TEXT("void.gate.crank"), By->GetPawn()->GetActorLocation());
}

// ================================================================================================ Vorhollow's stage

AWRVorhollowStage::AWRVorhollowStage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRVorhollowStage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVorhollowStage, SplitAt);
	DOREPLIFETIME(AWRVorhollowStage, RingUntil);
	DOREPLIFETIME(AWRVorhollowStage, WardUntil);
	DOREPLIFETIME(AWRVorhollowStage, BeamStart);
	DOREPLIFETIME(AWRVorhollowStage, BeamOrigin);
	DOREPLIFETIME(AWRVorhollowStage, BeamYaw0);
	DOREPLIFETIME(AWRVorhollowStage, BeamSweep);
	DOREPLIFETIME(AWRVorhollowStage, BeamRate);
	DOREPLIFETIME(AWRVorhollowStage, BeamStopAt);
	DOREPLIFETIME(AWRVorhollowStage, BeamStopFor);
	DOREPLIFETIME(AWRVorhollowStage, BeamLength);
	DOREPLIFETIME(AWRVorhollowStage, bGullet);
}

double AWRVorhollowStage::Now() const { return VbSNow(this); }

AWRVorhollowStage* AWRVorhollowStage::Find(const UWorld* World)
{
	for (TActorIterator<AWRVorhollowStage> It(World); It; ++It) return *It;
	return nullptr;
}

void AWRVorhollowStage::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		RingUntil.SetNumZeroed(6);
		WardUntil.SetNumZeroed(7);
	}
	Build();
}

void AWRVorhollowStage::Build()
{
	// The island: a 72 m disc of Choir paving on a rock body, a 30 m voidglass keel underneath. Seven pieces: the Stage
	// (the dais, 30 m) and six sectors of the outer ring, each its own frame (with collision) so it can split and orbit.
	// Six stepped stone rings rise toward the dais (the amphitheatre); six voice pillars stand on the third ring.
	UMaterialInterface* Pave = WRFoldLook::RockMat(this, VbDolo * 0.42f);
	UMaterialInterface* Rock = WRFoldLook::RockMat(this, FLinearColor(0.06f, 0.058f, 0.055f));
	UMaterialInterface* GlassM = WRFoldLook::RockMat(this, VbGlass);
	UMaterialInterface* Iron = WRFoldLook::RockMat(this, FLinearColor(0.035f, 0.033f, 0.035f));
	UMaterialInterface* Bronze = WRFoldLook::RockMat(this, FLinearColor(0.2f, 0.12f, 0.05f));
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const auto Solid = [&](USceneComponent* Parent, const TCHAR* Shape, const FVector& Size, const FVector& At, const FRotator& Rot, UMaterialInterface* M, bool bCollide)
	{
		UStaticMeshComponent* C = WRFoldLook::Shape(this, Parent, Shape, Size, At, Rot, M);
		if (!C) return C;
		if (bCollide) { C->SetCollisionProfileName(TEXT("BlockAll")); C->SetMobility(EComponentMobility::Movable); }
		if (!bDraw) C->SetVisibility(false);
		return C;
	};
	const float R = 3600.f, RStage = 1500.f;
	// The Stage piece: the dais and its rings, the keel.
	{
		USceneComponent* P = NewObject<USceneComponent>(this);
		P->SetupAttachment(RootComponent);
		P->SetMobility(EComponentMobility::Movable);
		P->RegisterComponent();
		Solid(P, TEXT("Cylinder"), FVector(2 * RStage, 2 * RStage, 120.f), FVector(0, 0, 540.f), FRotator::ZeroRotator, Pave, true);
		Solid(P, TEXT("Cylinder"), FVector(1200.f, 1200.f, 60.f), FVector(0, 0, 630.f), FRotator::ZeroRotator, Pave, true);   // the dais top
		Solid(P, TEXT("Cone"), FVector(2 * RStage, 2 * RStage, 1500.f), FVector(0, 0, -270.f), FRotator(180.f, 0.f, 0.f), Rock, false);
		Solid(P, TEXT("Cone"), FVector(500.f, 500.f, 3000.f), FVector(0, 0, -2400.f), FRotator(180.f, 0.f, 0.f), GlassM, false);   // the keel
		// The Void altar on the dais (it survives the split on the Stage piece).
		Solid(P, TEXT("Cube"), FVector(220.f, 140.f, 110.f), FVector(-300.f, 0, 715.f), FRotator::ZeroRotator, Pave, true);
		FPiece Pc;
		Pc.Root = P;
		Pc.Radius = 0.f;
		Pieces.Add(Pc);
	}
	for (int32 i = 0; i < 6; ++i)
	{
		USceneComponent* P = NewObject<USceneComponent>(this);
		P->SetupAttachment(RootComponent);
		P->SetMobility(EComponentMobility::Movable);
		P->RegisterComponent();
		const float A0 = 60.f * i, Mid = A0 + 30.f;
		// A sector of paving from RStage to R, built of slabs along arcs (each ring a step lower outward: the tiers).
		for (int32 ring = 0; ring < 6; ++ring)
		{
			const float R0 = RStage + (R - RStage) * ring / 6.f, R1 = RStage + (R - RStage) * (ring + 1) / 6.f;
			const float Z = 540.f - 70.f * ring;
			const int32 Slabs = 4 + ring;
			for (int32 s = 0; s < Slabs; ++s)
			{
				const float A = FMath::DegreesToRadians(A0 + 60.f * (s + 0.5f) / Slabs);
				const float Rm = (R0 + R1) * 0.5f;
				const float W = 2.f * Rm * FMath::Sin(FMath::DegreesToRadians(30.f / Slabs)) + 8.f;
				Solid(P, TEXT("Cube"), FVector(R1 - R0 + 6.f, W, 120.f), FVector(FMath::Cos(A) * Rm, FMath::Sin(A) * Rm, Z), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f), Pave, true);
			}
		}
		// The rock body under it and its own voidglass keel (pieces float by the floating rule).
		const float MA = FMath::DegreesToRadians(Mid);
		Solid(P, TEXT("Cone"), FVector(R - RStage, (R - RStage) * 1.2f, 1300.f), FVector(FMath::Cos(MA) * (RStage + R) * 0.5f, FMath::Sin(MA) * (RStage + R) * 0.5f, -300.f), FRotator(180.f, 0.f, 0.f), Rock, false);
		Solid(P, TEXT("Cone"), FVector(200.f, 200.f, 1400.f), FVector(FMath::Cos(MA) * (RStage + R) * 0.5f, FMath::Sin(MA) * (RStage + R) * 0.5f, -1300.f), FRotator(180.f, 0.f, 0.f), GlassM, false);
		// The keel-stone at its centre (an Arcane hit or anchor salt wards it): a black spike standing out of the paving.
		Solid(P, TEXT("Cone"), FVector(90.f, 90.f, 260.f), FVector(FMath::Cos(MA) * (RStage + R) * 0.55f, FMath::Sin(MA) * (RStage + R) * 0.55f, 560.f), FRotator::ZeroRotator, GlassM, true);
		// The voice pillar on the third ring: a Choir stone horn 4 m tall, 1.2 m wide, its mouth up.
		const float PR = RStage + (R - RStage) * 2.5f / 6.f;
		const FVector PL(FMath::Cos(MA) * PR, FMath::Sin(MA) * PR, 540.f - 70.f * 2);
		Solid(P, TEXT("Cylinder"), FVector(120.f, 120.f, 400.f), PL + FVector(0, 0, 260.f), FRotator::ZeroRotator, Pave, true);
		Solid(P, TEXT("Cone"), FVector(170.f, 170.f, 120.f), PL + FVector(0, 0, 520.f), FRotator(180.f, 0.f, 0.f), Pave, false);
		PillarPiece.Add(i + 1);
		PillarLocal.Add(PL + FVector(0, 0, 260.f));
		// Its rim eye for a chain (N, E, S, W pieces).
		FPiece Pc;
		Pc.Root = P;
		Pc.Angle = Mid;
		Pc.Radius = (RStage + R) * 0.5f;
		Pc.Gap = 150.f + 200.f * ((i * 37) % 6) / 5.f;
		Pc.bSmall = i == 1 || i == 4;
		Pieces.Add(Pc);
	}
	// The four chains from rim eyes (N, E, S, W) up to the anchor stones on the lip, 60 cm links, sagging 3 m mid-span.
	const float LipRise = LipZ - GetActorLocation().Z;
	for (int32 c = 0; c < 4; ++c)
	{
		const float A = FMath::DegreesToRadians(90.f * c);
		const FVector Eye(FMath::Cos(A) * R, FMath::Sin(A) * R, 480.f);
		const FVector Anchor(FMath::Cos(A) * 16500.f, FMath::Sin(A) * 16500.f, LipRise + 300.f);
		USceneComponent* Chain = NewObject<USceneComponent>(this);
		Chain->SetupAttachment(RootComponent);
		Chain->RegisterComponent();
		const int32 Links = 60;
		for (int32 k = 0; k < Links; ++k)
		{
			const float T = (k + 0.5f) / Links;
			const FVector P = FMath::Lerp(Eye, Anchor, T) - FVector(0, 0, 300.f * 4.f * T * (1.f - T));
			const FRotator Rot = (Anchor - Eye).Rotation() + FRotator(0.f, 0.f, k % 2 ? 90.f : 0.f);
			if (bDraw) WRFoldLook::Shape(this, Chain, TEXT("Cylinder"), FVector(60.f, 18.f, 26.f), P, Rot + FRotator(90.f, 0.f, 0.f), Iron);
		}
		Chains.Add(Chain);
		// The anchor stone: a 6 m Choir block with a bronze eye and a cairn on top.
		Solid(RootComponent, TEXT("Cube"), FVector(600.f, 600.f, 600.f), Anchor + FVector(0, 0, 0.f), FRotator(0.f, 90.f * c, 0.f), Pave, true);
		Solid(RootComponent, TEXT("Cylinder"), FVector(160.f, 160.f, 40.f), Anchor + FVector(-FMath::Cos(A) * 310.f, -FMath::Sin(A) * 310.f, 0.f), FRotator(90.f, FMath::RadiansToDegrees(A), 0.f), Bronze, false);
		Solid(RootComponent, TEXT("Cone"), FVector(160.f, 160.f, 200.f), Anchor + FVector(0, 0, 400.f), FRotator(180.f, 0.f, 0.f), Pave, false);
	}
	// The chain bridge: 124 m from the west lip to the island's west rim, 2 x 1 m slabs on two chains with iron stirrups.
	{
		const FVector From(-16200.f, 0.f, LipRise), To(-R - 50.f, 0.f, 520.f);
		const int32 N = 124;
		for (int32 k = 0; k < N; ++k)
		{
			const float T = (k + 0.5f) / N;
			const FVector P = FMath::Lerp(From, To, T) - FVector(0, 0, 500.f * 4.f * T * (1.f - T));
			Solid(RootComponent, TEXT("Cube"), FVector(96.f, 200.f, 14.f), P, (To - From).Rotation(), Pave, true);
			if (bDraw && k % 2 == 0)
			{
				for (const float Side : { -1.f, 1.f })
				{
					WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(8.f, 8.f, 100.f), P + FVector(0, Side * 110.f, 40.f), FRotator(0.f, 0.f, 0.f), Iron);
					WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(10.f, 10.f, 200.f), P + FVector(0, Side * 110.f, 90.f), FRotator(90.f, 0.f, 0.f) + (To - From).Rotation(), Iron);
				}
			}
		}
	}
	// The beam: a long thin shaft of violet-white (shown while it burns), and the Gullet below (shown when it opens).
	if (bDraw)
	{
		Beam = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(150.f, 150.f, 4000.f), FVector::ZeroVector, FRotator(90.f, 0.f, 0.f), WRFoldLook::Glow(this, VbViolet * 3.f, 6.f));
		if (Beam) Beam->SetVisibility(false, true);
	}
	Gullet = NewObject<USceneComponent>(this);
	Gullet->SetupAttachment(RootComponent);
	Gullet->RegisterComponent();
	Gullet->SetRelativeLocation(FVector(0, 0, -9000.f));
	// The Gullet: a 30 m chamber of ribbed glass walls, a floor of swallowed rubble and Choir bones, the Void heart on cords.
	Solid(Gullet, TEXT("Cylinder"), FVector(3000.f, 3000.f, 100.f), FVector(0, 0, -50.f), FRotator::ZeroRotator, Rock, true);
	for (int32 rib = 0; rib < 12; ++rib)
	{
		const float A = FMath::DegreesToRadians(30.f * rib);
		Solid(Gullet, TEXT("Cube"), FVector(120.f, 120.f, 1400.f), FVector(FMath::Cos(A) * 1500.f, FMath::Sin(A) * 1500.f, 700.f), FRotator(0.f, 30.f * rib, -8.f), GlassM, true);
	}
	Solid(Gullet, TEXT("Cylinder"), FVector(3200.f, 3200.f, 100.f), FVector(0, 0, 1450.f), FRotator::ZeroRotator, GlassM, true);
	for (int32 b = 0; b < 30; ++b) if (bDraw) WRFoldLook::Shape(this, Gullet, TEXT("Cube"), FVector(60.f, 20.f, 14.f), FVector(FMath::Sin(b * 2.3f) * 1100.f, FMath::Cos(b * 1.7f) * 1100.f, 8.f), FRotator(0.f, b * 37.f, 0.f), Pave);
	if (bDraw) WRFoldLook::Light(this, Gullet, FVector(0, 0, 900.f), VbViolet, 6000.f, 2600.f);
	Gullet->SetVisibility(false, true);
	if (HasAuthority())
	{
		// The Void heart hangs at the centre (it shares Vorhollow's health).
		if (AWRCreature* H = AWRCreature::SpawnCreature(GetWorld(), TEXT("void_heart"), GetActorLocation() + FVector(0, 0, -9000.f + 400.f), FRotator::ZeroRotator))
		{
			H->GetCharacterMovement()->DisableMovement();
		}
	}
	// The island's own light: the rift glow from below, the dais's braziers.
	if (bDraw)
	{
		WRFoldLook::Light(this, RootComponent, FVector(0, 0, -1500.f), VbViolet, 12000.f, 6000.f);
		WRFoldLook::Light(this, RootComponent, FVector(0, 0, 1200.f), FLinearColor(1.f, 0.75f, 0.5f), 4000.f, 3000.f);
	}
}

FTransform AWRVorhollowStage::PieceNow(int32 i, double T) const
{
	if (!Pieces.IsValidIndex(i)) return FTransform::Identity;
	const FPiece& P = Pieces[i];
	// Whole: every piece in its place, the island bobbing 0.3 m on a 10 s cycle.
	const float Bob = 30.f * FMath::Sin(2.f * PI * float(FMath::Fmod(T, 10.0)) / 10.f + i * 0.9f);
	if (SplitAt <= 0.0 || T < SplitAt) return FTransform(FRotator::ZeroRotator, FVector(0, 0, Bob));
	// Split: apart from 0.5 m/s, easing to its orbit over 6 s; the outer pieces orbit the centre at 1.2 m/s, their gaps
	// breathing 1.5-3.5 m (neighbours touch and grind every 20 s); the two smallest fall away after 90 s and rise again 30 s
	// later on their keels. All a function of the clock: every machine shows the same, nothing snaps.
	const float Since = float(T - SplitAt);
	const float Ease = WRTune(TEXT("bosses.vorhollow.splitEase"), 6.f);
	const float Out = Since < Ease ? 0.5f * Since * Since / (2.f * Ease) * 100.f : (0.5f * Ease / 2.f + 0.f) * 100.f + 0.f;
	if (i == 0) return FTransform(FRotator(0.f, Since * 2.f, 0.f), FVector(0, 0, Bob));
	const float Orbit = WRTune(TEXT("bosses.vorhollow.orbit"), 1.2f) * 100.f;
	const float W = FMath::Clamp(Since / Ease, 0.f, 1.f);
	const float Gap = P.Gap * (0.6f + 0.4f * FMath::Sin(2.f * PI * Since / 20.f + i));
	const float Rr = P.Radius + (Out + Gap) * W;
	const float Ang = Since > Ease ? (Since - Ease * 0.5f) * Orbit / FMath::Max(1.f, Rr) : 0.5f * Since * Since / Ease * Orbit / FMath::Max(1.f, Rr);
	const FVector Base(FMath::Cos(FMath::DegreesToRadians(P.Angle)) * P.Radius, FMath::Sin(FMath::DegreesToRadians(P.Angle)) * P.Radius, 0.f);
	const FVector Now2(FMath::Cos(FMath::DegreesToRadians(P.Angle) + Ang) * Rr, FMath::Sin(FMath::DegreesToRadians(P.Angle) + Ang) * Rr, 0.f);
	float Drop = 0.f;
	if (P.bSmall)
	{
		const float C = FMath::Fmod(Since, 120.f);
		const bool bHeld = WardUntil.IsValidIndex(i) && WardUntil[i] > SplitAt + (Since - C) + 90.0;
		if (!bHeld) Drop = C < 90.f ? 0.f : C < 96.f ? -5000.f * FMath::InterpEaseIn(0.f, 1.f, (C - 90.f) / 6.f, 2.f) : C < 114.f ? -5000.f : -5000.f * (1.f - FMath::InterpEaseOut(0.f, 1.f, (C - 114.f) / 6.f, 2.f));
	}
	// The piece turns with its orbit (the rotation about the centre): position relative to the stage, rotation about its own frame.
	const FQuat Q(FVector::UpVector, Ang);
	return FTransform(Q, Now2 - Q.RotateVector(Base) + FVector(0, 0, Bob + Drop));
}

void AWRVorhollowStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T = Now();
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		if (USceneComponent* C = Pieces[i].Root.Get())
		{
			const FTransform X = PieceNow(i, T);
			C->SetRelativeLocationAndRotation(X.GetLocation(), X.GetRotation());
		}
	}
	// The chains snap one after another, 0.3 s apart, each at a link (they go: the free end into the black).
	const bool bSplit = SplitAt > 0.0 && T >= SplitAt;
	for (int32 c = 0; c < Chains.Num(); ++c)
	{
		const bool bShow = !bSplit || T < SplitAt + 0.3 * c;
		if (Chains[c] && Chains[c]->IsVisible() != bShow) Chains[c]->SetVisibility(bShow, true);
	}
	// The beam.
	float Yaw = 0.f;
	const bool bBeam = BeamNow(Yaw);
	if (Beam)
	{
		if (Beam->IsVisible() != bBeam) Beam->SetVisibility(bBeam, true);
		if (bBeam)
		{
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			Beam->SetWorldLocationAndRotation(FVector(BeamOrigin) + Dir * BeamLength * 0.5f, FRotator(90.f, Yaw, 0.f) + FRotator(0.f, 0.f, 0.f));
			Beam->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
			Beam->SetWorldScale3D(FVector(1.5f, 1.5f, BeamLength / 100.f));
		}
	}
	if (Gullet && Gullet->IsVisible() != bGullet) Gullet->SetVisibility(bGullet, true);
	if (!HasAuthority()) return;
	// A heavy, charged or Wildbrand blow on a voice pillar rings it (3 s).
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || !FWRVoidBoss::HeavySwing(*It)) continue;
		for (int32 i = 0; i < PillarLocal.Num(); ++i)
		{
			const FVector To = PillarAt(i) - It->GetActorLocation();
			if (To.Size2D() < 260.f && FVector::DotProduct(It->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D()) > 0.4f && !PillarRinging(i)) StrikePillar(PillarAt(i), 10.f);
		}
	}
	// Falling into the black (inside the rift, past the void floor): Unmade.
	VoidClock += DeltaSeconds;
	if (VoidClock > 0.5f)
	{
		VoidClock = 0.f;
		const float Floor = LipZ - WRTune(TEXT("voidBosses.voidFloor"), 19000.f);
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !WRLand::InHole(It->GetActorLocation()) || It->GetActorLocation().Z > Floor || InGullet.Contains(*It)) continue;
			VbHurt(this, *It, 99999.f, 5, true, It->GetActorLocation());
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("Unmade: the black took you."));
		}
	}
	// The Gullet (phase 3): a constant pull toward the open chest; inside, the drain, the crushing ribs, the cough.
	if (bGullet)
	{
		AWRBoss* V = nullptr;
		for (TActorIterator<AWRBoss> It(GetWorld()); It; ++It) if (It->CreatureId == TEXT("vorhollow") && It->IsAlive()) V = *It;
		if (!V) { OpenGullet(false); return; }
		const FVector Hole = V->GetActorLocation() + V->GetActorForwardVector() * V->GetSimpleCollisionRadius() * 0.3f;
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || InGullet.Contains(*It)) continue;
			const FVector To = Hole - It->GetActorLocation();
			if (To.Size2D() > 6000.f) continue;
			It->LaunchCharacter(To.GetSafeNormal2D() * WRTune(TEXT("bosses.vorhollow.p3Pull"), 200.f), true, false);
			if (To.Size() < 600.f)
			{
				// In through the chest hole: the Gullet.
				InGullet.Add(*It);
				It->TeleportTo(GulletCentre() + FVector(800.f, 0.f, 200.f), It->GetActorRotation(), false, true);
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("Inside it: strike the heart, keep to the alcoves between the ribs"));
			}
		}
		GulletClock += DeltaSeconds;
		CoughClock += DeltaSeconds;
		for (auto It = InGullet.CreateIterator(); It; ++It)
		{
			AWRCharacter* C = Cast<AWRCharacter>(It->Get());
			if (!C || !C->IsAlive()) { It.RemoveCurrent(); continue; }
			WRVoid::LoseMaxHealth(C, WRTune(TEXT("bosses.vorhollow.gulletDrain"), 3.f) * DeltaSeconds * WRVoid::RiftDrainMult(C), this, true);
		}
		if (GulletClock >= WRTune(TEXT("bosses.vorhollow.gulletCrushEvery"), 20.f))
		{
			// The ribs creak and close (1.2 s tell: the sound and the walls' shudder, then the ring): the alcoves are safe.
			GulletClock = 0.f;
			WRSound::PlayAt(this, TEXT("void.gullet.creak"), GulletCentre());
			TWeakObjectPtr<AWRVorhollowStage> Self(this);
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, [Self]()
			{
				if (!Self.IsValid()) return;
				for (const TWeakObjectPtr<AActor>& W : Self->InGullet)
				{
					AWRCharacter* C = Cast<AWRCharacter>(W.Get());
					if (!C) continue;
					const FVector L = C->GetActorLocation() - Self->GulletCentre();
					const float Ang = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X)) + 360.f, 30.f);
					const bool bAlcove = L.Size2D() > 1300.f && Ang > 8.f && Ang < 22.f;
					if (!bAlcove) VbHurt(Self.Get(), C, WRTune(TEXT("bosses.vorhollow.gulletCrush"), 60.f), 4, true, Self->GulletCentre());
				}
				WRFx::Broadcast(Self.Get(), TEXT("void.collapse"), Self->GulletCentre(), FRotator::ZeroRotator, 3.f, TEXT("void.crush"));
			}, 1.2f, false);
		}
		if (CoughClock >= WRTune(TEXT("bosses.vorhollow.gulletCoughEvery"), 30.f))
		{
			// It coughs: everyone inside is thrown out onto the Stage, knocked down.
			CoughClock = 0.f;
			for (const TWeakObjectPtr<AActor>& W : InGullet)
			{
				if (AWRCharacter* C = Cast<AWRCharacter>(W.Get()))
				{
					C->TeleportTo(StageTop() + FVector(FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f), 200.f), C->GetActorRotation(), false, true);
					if (C->Status) C->Status->Stagger(1.5f);
				}
			}
			InGullet.Reset();
			WRSound::PlayAt(this, TEXT("void.gullet.cough"), StageTop());
		}
	}
}

void AWRVorhollowStage::Split(bool bWant)
{
	if (!HasAuthority()) return;
	SplitAt = bWant ? Now() : 0.0;
	if (bWant)
	{
		WRSound::PlayAt(this, TEXT("void.chain.snap"), GetActorLocation());
		WRFx::Broadcast(this, TEXT("void.crush"), GetActorLocation() + FVector(0, 0, 600.f), FRotator::ZeroRotator, 8.f, TEXT("void.island.split"));
	}
}

FVector AWRVorhollowStage::PillarAt(int32 i) const
{
	if (!PillarLocal.IsValidIndex(i) || !Pieces.IsValidIndex(PillarPiece[i])) return GetActorLocation();
	const FTransform X = PieceNow(PillarPiece[i], Now()) * GetActorTransform();
	return X.TransformPosition(PillarLocal[i]);
}

bool AWRVorhollowStage::PillarRinging(int32 i) const
{
	return RingUntil.IsValidIndex(i) && Now() < RingUntil[i];
}

bool AWRVorhollowStage::StrikePillar(const FVector& At, float Radius)
{
	if (!HasAuthority()) return false;
	for (int32 i = 0; i < PillarLocal.Num(); ++i)
	{
		if (FVector::Dist(PillarAt(i), At) > 200.f + Radius) continue;
		// It rings for 3 s: the Choir's song still works for anyone.
		RingUntil[i] = Now() + WRTune(TEXT("bosses.vorhollow.pillarRing"), 3.f);
		WRFx::Broadcast(this, TEXT("void.hush.scream"), PillarAt(i) + FVector(0, 0, 260.f), FRotator::ZeroRotator, 0.8f, TEXT("void.pillar.ring"));
		return true;
	}
	return false;
}

bool AWRVorhollowStage::PillarBetween(const FVector& From, const FVector& To) const
{
	for (int32 i = 0; i < PillarLocal.Num(); ++i)
	{
		const FVector P = PillarAt(i);
		const FVector Q = FMath::ClosestPointOnSegment(P, From, To);
		if (FVector::Dist2D(P, Q) < 90.f && (Q - From).Size() > 50.f) return true;
	}
	return false;
}

void AWRVorhollowStage::StartBeam(const FVector& Origin, float Yaw0, float Sweep, float Rate, float StopAt, float StopFor, float Length)
{
	BeamStart = Now();
	BeamOrigin = Origin;
	BeamYaw0 = Yaw0;
	BeamSweep = Sweep;
	BeamRate = FMath::Max(1.f, Rate);
	BeamStopAt = StopAt;
	BeamStopFor = StopFor;
	BeamLength = Length;
	WRSound::PlayAt(this, TEXT("void.beam"), Origin);
}

void AWRVorhollowStage::EndBeam()
{
	BeamStart = 0.0;
}

bool AWRVorhollowStage::BeamNow(float& OutYaw) const
{
	if (BeamStart <= 0.0) return false;
	float T = float(Now() - BeamStart);
	if (BeamStopAt > 0.f && T > BeamStopAt) T = T < BeamStopAt + BeamStopFor ? BeamStopAt : T - BeamStopFor;
	const float Swept = T * BeamRate;
	if (Swept > BeamSweep) return false;
	OutYaw = BeamYaw0 + Swept;
	return true;
}

void AWRVorhollowStage::WardAt(const FVector& At, float Seconds)
{
	if (!HasAuthority()) return;
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		const FTransform X = PieceNow(i, Now()) * GetActorTransform();
		const FVector C = X.TransformPosition(FVector(FMath::Cos(FMath::DegreesToRadians(Pieces[i].Angle)) * Pieces[i].Radius, FMath::Sin(FMath::DegreesToRadians(Pieces[i].Angle)) * Pieces[i].Radius, 560.f));
		if (FVector::Dist2D(C, At) < 1200.f && WardUntil.IsValidIndex(i))
		{
			WardUntil[i] = Now() + Seconds;
			WRFx::Broadcast(this, TEXT("void.unravel"), C, FRotator::ZeroRotator, 2.f, TEXT("void.ward"));
		}
	}
}

void AWRVorhollowStage::OpenGullet(bool bOpen)
{
	if (!HasAuthority()) return;
	bGullet = bOpen;
	GulletClock = CoughClock = 0.f;
	if (!bOpen)
	{
		for (const TWeakObjectPtr<AActor>& W : InGullet)
		{
			if (AWRCharacter* C = Cast<AWRCharacter>(W.Get())) C->TeleportTo(StageTop() + FVector(0, 0, 200.f), C->GetActorRotation(), false, true);
		}
		InGullet.Reset();
	}
}

FVector AWRVorhollowStage::GulletCentre() const
{
	return GetActorLocation() + FVector(0, 0, -9000.f);
}

FVector AWRVorhollowStage::StageTop() const
{
	return GetActorLocation() + FVector(0, 0, 700.f);
}
