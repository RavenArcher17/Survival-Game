// The Crystal caves' bosses: their counters, own moves and the hooks AWRBoss calls (cooks/biome-crystal.md 2.13-2.14;
// cooks/build-biome-crystal.md Part 2 P2.3-P2.7). See WRCrystalBosses.h. The machines' and bodies' looks are
// WRCrystalBossLook.cpp; Lyrathis and her hall WRCrystalLyrathis.cpp.
//
// Every blow here goes through UWRCombatLibrary::ApplyHit, and only where the body (or the thing it threw) really is:
// lobs fly a gravity arc from where they left and never home, nothing lands inside 4 m, a beam stops at what it meets.
// Moving things are eased on the server's clock: the double walks (never pops), the swap is a 1 s slide along the lens's
// light, the Tyrant walks its tubes.

#include "WRCrystalBosses.h"
#include "WRCrystalBossState.h"
#include "Wildreach.h"
#include "WRArcanePath.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRAltar.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRCrystalTypes.h"
#include "WRCrystalWorld.h"
#include "WRCrystalHazards.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Algo/Reverse.h"


namespace WRCrystalBossImpl
{
	TMap<TWeakObjectPtr<const AWRBoss>, FWRCrystalBossState>& CbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRCrystalBossState> Map;
		return Map;
	}
	FWRCrystalBossState& CbS(const AWRBoss* Boss) { return CbStates().FindOrAdd(Boss); }
}
FWRCrystalBossState& WRCrystalBossStateOf(const AWRBoss* Boss) { return WRCrystalBossImpl::CbStates().FindOrAdd(Boss); }
FWRCrystalBossState* WRCrystalBossFind(const AWRBoss* Boss) { return WRCrystalBossImpl::CbStates().Find(Boss); }
namespace WRCrystalBossImpl
{
	double CbNow(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }
	float CbT(const AWRBoss* Boss, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback); }
	float CbDamageScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }
	FName CbKindOf(const AWRBoss* Boss)
	{
		if (!Boss) return NAME_None;
		const FName Id = Boss->CreatureId;
		return Id == TEXT("mirrored_twins") || Id == TEXT("echo_warden") || Id == TEXT("geode_tyrant") || Id == TEXT("lyrathis") ? Id : NAME_None;
	}
	FVector CbFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }
	bool CbGround(const UWorld* World, const FVector& At, FVector& Out, float Up = 800.f, float Down = 3000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRCrystalBossGround), false);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}
	TArray<AWRCharacterBase*> CbFoes(const AWRBoss* Boss, float Range, const FVector& From)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || FWRCrystalBoss::IsDouble(*It)) continue;
			if (FVector::Dist2D(It->GetActorLocation(), From) > Range + It->GetSimpleCollisionRadius()) continue;
			if (FMath::Abs(It->GetActorLocation().Z - From.Z) > 1200.f) continue;
			Out.Add(*It);
		}
		return Out;
	}
	void CbTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** A blow that isn't the row's own strike (the double's, a hammer ring, a shard), with the boss's scaling. */
	void CbHitWith(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, bool bParryable, bool bLow, const FVector& Origin, FName Buildup = NAME_None, float Amount = 0.f, bool bMelee = true)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * CbDamageScale() * (Boss->Phase >= 3 ? CbT(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = Poise;
		Hit.Impact = Impact;
		Hit.bParryable = bParryable && bMelee;
		Hit.bMelee = bMelee;
		Hit.bLow = bLow;
		Hit.bHeavy = Damage >= 30.f;
		Hit.Tier = Damage >= 30.f ? EWRHitTier::Heavy : EWRHitTier::Light;
		Hit.Origin = Origin;
		Hit.Element = Buildup == TEXT("mark") ? WRTags::Element_Arcane : WRTags::Element_Physical;
		Hit.PhysType = 2;
		if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	float CbHealthFraction(const AWRBoss* Boss)
	{
		const UWRAttributeSet* A = Boss->GetAttributes();
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}
	FVector CbHitFrom(const AActor* Boss, const FWRHit& Hit)
	{
		if (!Hit.ImpactPoint.IsZero()) return Hit.ImpactPoint;
		if (!Hit.Origin.IsZero()) return Hit.Origin;
		return Hit.Instigator ? Hit.Instigator->GetActorLocation() : Boss->GetActorLocation();
	}
	/** Ease in and out (0..1). */
	float CbEase(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }
	/** A point kept inside the arena's floor (r - margin from its centre). */
	FVector CbInside(const FWRCrystalBossState& St, const FVector& P, float Margin)
	{
		const FVector D = P - St.Home;
		const float R = FMath::Max(100.f, St.ArenaR - Margin);
		return D.Size2D() <= R ? P : St.Home + D.GetSafeNormal2D() * R + FVector(0.f, 0.f, D.Z);
	}
	/** Move a character kinematically toward To at Speed (cm/s), facing Face, setting its velocity for its animation. */
	void CbSlide(ACharacter* C, const FVector& To, float Speed, float Dt, const FVector& Face)
	{
		const FVector From = C->GetActorLocation();
		FVector Next = FMath::VInterpConstantTo(From, FVector(To.X, To.Y, From.Z), Dt, Speed);
		FVector G;
		if (CbGround(C->GetWorld(), Next, G, 200.f, 600.f)) Next.Z = G.Z + C->GetSimpleCollisionHalfHeight() + 2.f;
		C->SetActorLocation(Next, false, nullptr, ETeleportType::None);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement()) M->Velocity = Dt > 0.f ? (Next - From) / Dt : FVector::ZeroVector;
		const FVector F = (Face - Next).GetSafeNormal2D();
		if (!F.IsNearlyZero()) C->SetActorRotation(FRotator(0.f, F.Rotation().Yaw, 0.f));
	}
	/** Is a light within Range of At (a held light, a lit Lumenhorn) for the Tyrant's glare. */
	bool CbLightNear(const UWorld* World, const FVector& At, float Range)
	{
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), At) > Range) continue;
			const UWRSurvivalComponent* S = UWRSurvivalComponent::For(*It);
			if ((S && S->IsLightOn()) || (Cast<AWRCreature>(*It) && Cast<AWRCreature>(*It)->CreatureId == TEXT("lumenhorn"))) return true;
		}
		return false;
	}
}
using namespace WRCrystalBossImpl;

// ================================================================================================ the hooks

bool FWRCrystalBoss::IsCrystalBoss(const AWRBoss* Boss)
{
	return !CbKindOf(Boss).IsNone();
}

bool FWRCrystalBoss::IsDouble(const AActor* Who)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	return C && C->CreatureId == TEXT("mirrored_twin_double");
}

bool FWRCrystalBoss::ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw)
{
	const FWRCrystalData& D = FWRCrystalData::Get();
	for (const FWRCrystalData::FHall& H : D.Halls)
	{
		if (H.Arena == BossId || H.Lair == BossId)
		{
			Centre = H.At;
			Radius = FMath::Max(H.Rx, H.Ry);
			Yaw = H.Yaw;
			return true;
		}
	}
	return false;
}

void FWRCrystalBoss::Begin(AWRBoss* Boss)
{
	if (!IsCrystalBoss(Boss)) return;
	UWRCrystalBossBody::Attach(Boss);
	if (!Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	St.Kind = CbKindOf(Boss);
	St.bBegun = true;
	if (!ArenaOf(St.Kind, St.Home, St.ArenaR, St.Yaw)) St.Home = Boss->GetActorLocation();
	const double T = CbNow(Boss);
	for (const TWeakObjectPtr<AWRCrystalBossProp>& W : AWRCrystalBossProp::All())
	{
		AWRCrystalBossProp* P = W.Get();
		if (!P || FVector::Dist2D(P->GetActorLocation(), St.Home) > St.ArenaR + 3000.f) continue;
		if (St.Kind == TEXT("mirrored_twins") && P->Kind == TEXT("lens")) St.Lens = P;
		if (St.Kind == TEXT("mirrored_twins") && P->Kind == TEXT("magazine")) St.Magazine = P;
		if (St.Kind == TEXT("echo_warden") && P->Kind == TEXT("chime")) St.Chimes.Add(P);
		if (St.Kind == TEXT("lyrathis") && P->Kind == TEXT("turntable")) St.Turntables.Add(P);
		P->OwnerBoss = Boss;
	}
	St.Turntables.Sort([](const TWeakObjectPtr<AWRCrystalBossProp>& A, const TWeakObjectPtr<AWRCrystalBossProp>& B) { return A.IsValid() && B.IsValid() && A->Index < B->Index; });
	St.FacedAt.Init(-100.0, 8);
	for (int32 r = 0; r < 6; ++r) St.RowHP[r] = CbT(Boss, TEXT("rowHealth"), 300.f);
	St.NextSwapAt = T + CbT(Boss, TEXT("swapEvery"), 12.f);
	if (St.Kind == TEXT("echo_warden"))
	{
		// Hearing: every noise the caves' model makes (steps, rolls, landings, blows, casts, chimes, thrown stones).
		if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Boss))
		{
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			St.Hearing = W->OnNoise.AddLambda([Weak](const FVector& At, float Radius, AActor* By, FName Kind)
			{
				AWRBoss* B = Weak.Get();
				if (!B || !B->IsAlive() || By == B) return;
				FWRCrystalBossState* S = CbStates().Find(B);
				if (!S) return;
				// It hears what reaches it: the noise's own radius must reach its ears, within its hall's 30 m.
				const float D = float(FVector::Dist(At, B->GetActorLocation()));
				if (D > CbT(B, TEXT("hearRange"), 3000.f) || D > Radius + B->GetSimpleCollisionRadius()) return;
				FWRCrystalBossState::FNoise N;
				N.At = At;
				N.Radius = Radius;
				N.When = CbNow(B);
				N.By = By;
				N.Kind = Kind;
				S->Heard.Add(N);
			});
		}
	}
	if (St.Kind == TEXT("geode_tyrant") && TyrantRoute().Num() > 0)
	{
		// It starts its round where it stands (the nearest waypoint).
		float Best = 1.e12f;
		const TArray<FVector>& R = TyrantRoute();
		for (int32 i = 0; i < R.Num(); ++i) { const float D = FVector::DistSquared(R[i], Boss->GetActorLocation()); if (D < Best) { Best = D; St.RouteIndex = i; } }
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL boss %s begins (arena %s r %.0f)"), *Boss->GetDisplayLabel(), *St.Home.ToCompactString(), St.ArenaR);
}

void FWRCrystalBoss::Reset(AWRBoss* Boss)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	const double T = CbNow(Boss);
	UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
	if (Body) { Body->Pose = 0; Body->Broken = 0; Body->Level = 0.f; Body->Gem = 0.f; }
	// The Twins: the double back to its place, the lens whole, the magazine full.
	if (AWRCreature* D = St.Double.Get()) { D->Destroy(); }
	St.Double = nullptr;
	St.bDoubleGone = false;
	St.SwapStart = -100.0;
	St.NextSwapAt = T + CbT(Boss, TEXT("swapEvery"), 12.f);
	if (AWRCrystalBossProp* L = St.Lens.Get()) { L->SetState(0); L->Hits = 0; }
	if (AWRCrystalBossProp* M = St.Magazine.Get()) { M->Value = CbT(Boss, TEXT("spares"), 3.f); M->LowerAt = -100.f; M->ForceNetUpdate(); }
	// The Warden: what it heard is forgotten; the chimes stay as they are (broken ones are broken till it's killed).
	St.Heard.Reset();
	St.HammerUntil = 0.0;
	St.bGoal = St.bBlastDue = St.bSnapDue = false;
	// The Tyrant: its plates whole again (a new fight, a healed beast).
	for (int32 r = 0; r < 6; ++r) St.RowHP[r] = CbT(Boss, TEXT("rowHealth"), 300.f);
	// Lyrathis (WRCrystalLyrathis.cpp).
	if (St.Kind == TEXT("lyrathis"))
	{
		ReleaseGrasp(Boss, St, false);
		if (AWRPrismBeam* B = St.Beam.Get()) B->Destroy();
		if (AWRPrismBeam* B = St.Beam2.Get()) B->Destroy();
		for (const TWeakObjectPtr<AWRCreature>& C : St.Copies) if (C.IsValid()) C->Destroy();
		St.Copies.Reset();
		St.bDome = St.bDomePermanent = St.bWard85 = St.bWard65 = false;
		St.StaggerUntil = 0.0;
		for (const TWeakObjectPtr<AWRCrystalBossProp>& P : St.Turntables) if (P.IsValid()) { P->Clicks = 0; P->TurnAt = -100.f; P->ForceNetUpdate(); }
	}
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Walking);
}

void FWRCrystalBoss::EndPlay(AWRBoss* Boss)
{
	if (FWRCrystalBossState* St = CbStates().Find(Boss))
	{
		if (St->Hearing.IsValid()) if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Boss)) W->OnNoise.Remove(St->Hearing);
		if (AWRCreature* D = St->Double.Get()) D->Destroy();
		if (AWRPrismBeam* B = St->Beam.Get()) B->Destroy();
		if (AWRPrismBeam* B = St->Beam2.Get()) B->Destroy();
		for (const TWeakObjectPtr<AWRCreature>& C : St->Copies) if (C.IsValid()) C->Destroy();
	}
	CbStates().Remove(Boss);
}

void FWRCrystalBoss::Died(AWRBoss* Boss)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	// The double goes out with its knight (the lens's light dies), the copies with her.
	if (AWRCreature* D = St.Double.Get())
	{
		WRFx::Broadcast(D, TEXT("crystal.double_fade"), D->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("crystal.boss.double_fade"));
		D->Destroy();
	}
	St.Double = nullptr;
	if (AWRCrystalBossProp* L = St.Lens.Get()) L->SetState(2);
	ReleaseGrasp(Boss, St, false);
	for (const TWeakObjectPtr<AWRCreature>& C : St.Copies) if (C.IsValid()) C->Destroy();
	St.Copies.Reset();
	if (AWRPrismBeam* B = St.Beam.Get()) B->Destroy();
	if (AWRPrismBeam* B = St.Beam2.Get()) B->Destroy();
	// The broken chimes ring whole again for the next Warden (its spawner brings it back).
	if (St.Kind == TEXT("echo_warden")) for (const TWeakObjectPtr<AWRCrystalBossProp>& C : St.Chimes) if (C.IsValid()) { C->Value = 0.f; C->SetState(0); }
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL boss %s falls"), *Boss->GetDisplayLabel());
}

bool FWRCrystalBoss::Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target)
{
	if (!Boss || !Target || Boss->Current != INDEX_NONE) return false;
	const int32 i = Boss->Attacks.IndexOfByPredicate([Id](const FWRBossAttack& A) { return A.Id == Id; });
	if (!Boss->Attacks.IsValidIndex(i) || Boss->Attacks[i].MinPhase > Boss->Phase) return false;
	Boss->StartAttack(Boss->Attacks[i], Target);
	return true;
}

void FWRCrystalBoss::CancelAttack(AWRBoss* Boss)
{
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike) EndStrike(Boss, Boss->Attacks[Boss->Current]);
	Boss->Current = INDEX_NONE;
	Boss->StrikeState = AWRBoss::EStrike::None;
	Boss->SpeedBoost = 1.f;
}

bool FWRCrystalBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRCrystalBossState& St = CbS(Boss);
	if (!St.bBegun) Begin(Boss);
	const double T = CbNow(Boss);
	if (St.Kind == TEXT("echo_warden"))
	{
		// Blind: it wakes to a noise in its hall, never to being seen. Until then it walks a slow round of the hall, its
		// horns sweeping (the soft ping of its listening).
		bSkipWake = true;
		St.Heard.RemoveAll([&](const FWRCrystalBossState::FNoise& N) { return T - N.When > CbT(Boss, TEXT("hearSeconds"), 1.5f); });
		for (const FWRCrystalBossState::FNoise& N : St.Heard)
		{
			const AWRCharacterBase* By = Cast<AWRCharacterBase>(N.By.Get());
			if (N.Kind == TEXT("chime") || (By && Boss->IsHostileTo(By)))
			{
				Boss->Engage();
				UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Echo Warden hears %s and wakes"), *N.Kind.ToString());
				return true;
			}
		}
		const FVector Way = St.Home + FRotator(0.f, St.Yaw + St.RoundIndex * 90.f + 45.f, 0.f).Vector() * St.ArenaR * 0.42f;
		if (FVector::Dist2D(Way, Boss->GetActorLocation()) < 250.f) St.RoundIndex = (St.RoundIndex + 1) % 4;
		Boss->AddMovementInput((Way - Boss->GetActorLocation()).GetSafeNormal2D(), CbT(Boss, TEXT("roundInput"), 0.3f));
		if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Point = Way;
		return true;
	}
	if (St.Kind == TEXT("geode_tyrant"))
	{
		// Its round (crystal 2.13.3): the Wallow, the Chandelier Gallery, the Organ's east end, the agate rinds, and back;
		// it feeds at each stop and rests 2 game hours lying in the Wallow's hot pool.
		UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
		const TArray<FVector>& R = TyrantRoute();
		if (R.Num() < 2) return false;
		const float GameHour = WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f;
		if (T < St.RestUntil)
		{
			if (Body) Body->Pose = 1;
			return true;
		}
		if (Body && Body->Pose == 1) Body->Pose = 0;
		if (T < St.FeedUntil) return true;
		const FVector Goal = R[St.RouteIndex];
		if (FVector::Dist2D(Goal, Boss->GetActorLocation()) < 300.f)
		{
			// A stop: the route's ends and its marked halls are where it feeds (licking the crystal linings).
			const bool bEnd = St.RouteIndex == 0 || St.RouteIndex == R.Num() - 1;
			if (St.RouteIndex == 0 && St.RouteDir < 0)
			{
				St.RestUntil = T + CbT(Boss, TEXT("restHours"), 2.f) * GameHour;
				St.RouteDir = 1;
				WRSound::PlayAt(Boss, TEXT("crystal.boss.tyrant_rest"), Boss->GetActorLocation());
			}
			else if (bEnd || St.RouteIndex % FMath::Max(1, R.Num() / 6) == 0)
			{
				St.FeedUntil = T + CbT(Boss, TEXT("feedSeconds"), 40.f);
				Boss->PlaySwing(EWRSwingKind::Light, 1.4f);
				WRSound::PlayAt(Boss, TEXT("crystal.boss.tyrant_feed"), Boss->GetActorLocation());
			}
			if (St.RouteIndex == R.Num() - 1) St.RouteDir = -1;
			St.RouteIndex = FMath::Clamp(St.RouteIndex + St.RouteDir, 0, R.Num() - 1);
			return true;
		}
		Boss->SetHome(FVector(Goal.X, Goal.Y, Boss->GetActorLocation().Z));
		Boss->bReturning = false;
		Boss->AddMovementInput((Goal - Boss->GetActorLocation()).GetSafeNormal2D(), CbT(Boss, TEXT("roamInput"), 0.4f));
		return true;
	}
	return false;
}

AWRCharacterBase* FWRCrystalBoss::PickTarget(AWRBoss* Boss, AWRCharacterBase* Default)
{
	if (!Boss || Boss->CreatureId != TEXT("echo_warden")) return Default;
	FWRCrystalBossState* St = CbStates().Find(Boss);
	if (!St || !St->bGoal) return Default;
	// The survivor nearest the noise it's going to (it doesn't see; this is only who its blows are aimed past).
	AWRCharacterBase* Best = nullptr;
	float BestD = 1.e12f;
	for (AWRCharacterBase* C : CbFoes(Boss, St->ArenaR + 2000.f, St->Home))
	{
		const float D = float(FVector::DistSquared(C->GetActorLocation(), St->Goal));
		if (D < BestD) { BestD = D; Best = C; }
	}
	return Best ? Best : Default;
}

bool FWRCrystalBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRCrystalBossState& St = CbS(Boss);
	if (!St.bBegun) Begin(Boss);
	if (St.Kind == TEXT("mirrored_twins")) return TickTwins(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("echo_warden")) return TickWarden(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("geode_tyrant")) return TickTyrant(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("lyrathis")) return TickLyrathis(Boss, St, DeltaSeconds);
	return false;
}

// ================================================================================================ the Mirrored Twins

namespace WRCrystalBossImpl
{
	/** Where the double stands: the real knight's place mirrored through its target (real on your left, double on
	 *  your right), kept on the Archive's floor. */
	FVector CbMirror(const FWRCrystalBossState& St, const AWRBoss* Boss, const AActor* Target)
	{
		if (!Target) return CbInside(St, St.Home + (St.Home - Boss->GetActorLocation()), 300.f);
		const FVector T = Target->GetActorLocation();
		const FVector P = T + (T - Boss->GetActorLocation());
		return CbInside(St, FVector(P.X, P.Y, Boss->GetActorLocation().Z), 250.f);
	}
}

bool FWRCrystalBoss::TickTwins(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	const double T = CbNow(Boss);
	UWorld* World = Boss->GetWorld();
	AWRCrystalBossProp* Lens = St.Lens.Get();
	AWRCrystalBossProp* Mag = St.Magazine.Get();
	const float Flare = CbT(Boss, TEXT("flareSeconds"), 1.f);
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	if (!Target || !Target->IsAlive()) Target = Boss->PickBossTarget();
	// A spare lens arriving down the magazine's chain: the yoke takes it and the double comes back.
	if (Lens && Lens->State == 1 && Mag && Mag->LowerAt > 0.f && Lens->ServerNow() >= Mag->LowerAt + CbT(Boss, TEXT("spareSeconds"), 20.f))
	{
		Lens->SetState(0);
		Lens->Hits = 0;
		Mag->LowerAt = -100.f;
		Mag->ForceNetUpdate();
		St.bDoubleGone = false;
		St.NextSwapAt = T + CbT(Boss, TEXT("swapEvery"), 12.f);
		WRSound::PlayAt(Boss, TEXT("crystal.boss.lens_set"), Lens->GetActorLocation());
	}
	const bool bLensWhole = Lens ? Lens->State == 0 : true;
	// The double: made when the lens throws it (and gone while there's no lens).
	AWRCreature* Double = St.Double.Get();
	if (!Double && bLensWhole && !St.bDoubleGone)
	{
		const FVector At = CbMirror(St, Boss, Target);
		Double = AWRCreature::SpawnCreature(World, TEXT("mirrored_twin_double"), At - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), Boss->GetActorRotation());
		if (Double)
		{
			// Hard light: it doesn't think for itself, and nothing solid stops at it.
			if (AController* C = Double->GetController()) { C->UnPossess(); C->Destroy(); }
			if (UCharacterMovementComponent* M = Double->GetCharacterMovement()) { M->bRunPhysicsWithNoController = true; M->SetMovementMode(MOVE_Walking); }
			Double->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			Double->SetHome(At);
			UWRCrystalBossBody::Attach(Double);   // the same crystal plate, helm, sword and shield (and no reflection)
			St.Double = Double;
			WRFx::Broadcast(Boss, TEXT("crystal.double_form"), At, FRotator::ZeroRotator, 1.4f, TEXT("crystal.boss.double_form"));
		}
	}
	if (Double && (!bLensWhole || St.bDoubleGone))
	{
		WRFx::Broadcast(Boss, TEXT("crystal.double_fade"), Double->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("crystal.boss.double_fade"));
		Double->Destroy();
		St.Double = nullptr;
		Double = nullptr;
	}
	// The swap: every 12 s the gimbal turns the lens a quarter (1.0 s, the chain rattling over its sprocket, a high
	// chime, the lens flaring) and the two trade places along its light. Never mid-blow: it waits for the blow to end.
	if (Double && bLensWhole && T >= St.NextSwapAt && Boss->Current == INDEX_NONE && St.SwapStart < 0.0)
	{
		StartSwap(Boss, St);
	}
	if (St.SwapStart >= 0.0)
	{
		const float A = CbEase(float(T - St.SwapStart) / FMath::Max(0.1f, Flare));
		if (Double)
		{
			const FVector Face = Target ? Target->GetActorLocation() : St.Home;
			const FVector PA = FMath::Lerp(St.SwapA, St.SwapB, A), PB = FMath::Lerp(St.SwapB, St.SwapA, A);
			Boss->SetActorLocation(FVector(PA.X, PA.Y, Boss->GetActorLocation().Z), false, nullptr, ETeleportType::None);
			Double->SetActorLocation(FVector(PB.X, PB.Y, Double->GetActorLocation().Z), false, nullptr, ETeleportType::None);
			Boss->SetActorRotation(FRotator(0.f, (Face - PA).Rotation().Yaw, 0.f));
			Double->SetActorRotation(FRotator(0.f, (Face - PB).Rotation().Yaw, 0.f));
		}
		if (T - St.SwapStart >= Flare)
		{
			St.SwapStart = -100.0;
			St.NextSwapAt = T + CbT(Boss, TEXT("swapEvery"), 12.f);
			if (Lens) Lens->Hits = 0;
		}
		return true;   // no blows while the lens flares
	}
	// The double keeps its mirror place (walking, never popping), facing the target, while nothing's being struck.
	if (Double && Boss->StrikeState != AWRBoss::EStrike::Strike)
	{
		const FVector Mirror = CbMirror(St, Boss, Target);
		CbSlide(Double, Mirror, FMath::Max(CbT(Boss, TEXT("doubleCatchUp"), 700.f), Boss->GetVelocity().Size2D() * 1.2f), Dt, Target ? Target->GetActorLocation() : St.Home);
	}
	// The Ward guard: a hexafoil on both shields for 3 s.
	if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Pose = T < St.WardUntil ? 1 : 0;
	if (UWRCrystalBossBody* DB = Double ? UWRCrystalBossBody::Get(Double) : nullptr) DB->Pose = T < St.WardUntil ? 1 : 0;
	return false;
}

void FWRCrystalBoss::StartSwap(AWRBoss* Boss, FWRCrystalBossState& St)
{
	AWRCreature* Double = St.Double.Get();
	if (!Double) return;
	St.SwapStart = CbNow(Boss);
	St.SwapA = Boss->GetActorLocation();
	St.SwapB = Double->GetActorLocation();
	if (AWRCrystalBossProp* Lens = St.Lens.Get())
	{
		Lens->Clicks += 1;
		Lens->TurnAt = Lens->ServerNow();
		Lens->Hits = 0;
		Lens->ForceNetUpdate();
		WRSound::PlayAt(Boss, TEXT("crystal.boss.gimbal"), Lens->GetActorLocation());
		WRFx::Broadcast(Boss, TEXT("crystal.lens_flare"), Lens->GetActorLocation(), FRotator::ZeroRotator, 1.2f, TEXT("crystal.boss.lens_flare"), CbT(Boss, TEXT("flareSeconds"), 1.f));
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Twins swap"));
}

void FWRCrystalBoss::ShatterLens(AWRBoss* Boss, FWRCrystalBossState& St)
{
	AWRCrystalBossProp* Lens = St.Lens.Get();
	if (!Lens || Lens->State != 0) return;
	AWRCrystalBossProp* Mag = St.Magazine.Get();
	const int32 Spares = Mag ? FMath::RoundToInt(Mag->Value) : 0;
	Lens->SetState(Spares > 0 ? 1 : 2);
	WRFx::Broadcast(Boss, TEXT("crystal.lens_shatter"), Lens->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.lens_shatter"));
	St.bDoubleGone = true;
	St.SwapStart = -100.0;
	// The magazine lowers a spare down its chain (20 s; you hear it clank all the way) while there's one knight.
	if (Mag && Spares > 0)
	{
		Mag->Value = float(Spares - 1);
		Mag->LowerAt = Mag->ServerNow();
		Mag->ForceNetUpdate();
		WRSound::PlayAt(Boss, TEXT("crystal.boss.magazine"), Mag->GetActorLocation());
	}
	for (AWRCharacterBase* C : CbFoes(Boss, St.ArenaR + 1000.f, St.Home)) CbTell(C, Spares > 0 ? TEXT("The lens shatters: one knight, until the magazine lowers another") : TEXT("The last lens shatters: one knight for good"));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Twins' lens shattered (%d spares left)"), FMath::Max(0, Spares - 1));
}

void FWRCrystalBoss::DoubleStrike(AWRBoss* Boss, FWRCrystalBossState& St, const FWRBossAttack& Attack, float Dt)
{
	// The mirror rule: the double makes the same move at the same instant from its own side of you. Its blows are hard
	// light: as hard as the knight's, but a parry on them fails (it isn't there to stagger).
	AWRCreature* Double = St.Double.Get();
	if (!Double) return;
	const double T = CbNow(Boss);
	const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
	const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
	const FVector At = Double->GetActorLocation();
	if (St.bDoubleLine)
	{
		// Its lunge / bash runs its own line toward you and on (the pair converging from both sides).
		const FVector P = FMath::Lerp(St.DoubleFrom, St.DoubleTo, CbEase(A));
		Double->SetActorLocation(FVector(P.X, P.Y, At.Z), true, nullptr, ETeleportType::None);
		for (AWRCharacterBase* C : CbFoes(Boss, Double->GetSimpleCollisionRadius() + Attack.Size * 0.5f, Double->GetActorLocation()))
		{
			if (St.DoubleHits.Contains(C)) continue;
			St.DoubleHits.Add(C);
			CbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, Attack.bLow, Double->GetActorLocation());
		}
		return;
	}
	if (A < 0.35f) return;
	const FVector Fwd = Double->GetActorForwardVector().GetSafeNormal2D();
	for (AWRCharacterBase* C : CbFoes(Boss, Attack.Range + Double->GetSimpleCollisionRadius(), At))
	{
		if (St.DoubleHits.Contains(C)) continue;
		const FVector To = (C->GetActorLocation() - At).GetSafeNormal2D();
		if (Attack.Shape == EWRBossShape::Cone && FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To, Fwd)), -1.f, 1.f))) > Attack.Size) continue;
		St.DoubleHits.Add(C);
		CbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, Attack.bLow, At);
	}
	(void)Dt;
}

// ================================================================================================ the Echo Warden

namespace WRCrystalBossImpl
{
	/** The loudest noise it holds now (radius wins; a chime always counts as the loudest thing in the room). */
	const FWRCrystalBossState::FNoise* CbLoudest(const FWRCrystalBossState& St)
	{
		const FWRCrystalBossState::FNoise* Best = nullptr;
		for (const FWRCrystalBossState::FNoise& N : St.Heard)
		{
			const float Score = N.Radius + (N.Kind == TEXT("chime") ? 100000.f : 0.f);
			const float BestScore = Best ? Best->Radius + (Best->Kind == TEXT("chime") ? 100000.f : 0.f) : -1.f;
			if (Score > BestScore || (Score == BestScore && Best && N.When > Best->When)) Best = &N;
		}
		return Best;
	}
}

bool FWRCrystalBoss::TickWarden(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	const double T = CbNow(Boss);
	UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
	St.Heard.RemoveAll([&](const FWRCrystalBossState::FNoise& N) { return T - N.When > CbT(Boss, TEXT("hearSeconds"), 1.5f); });
	// Hammering a chime: four blows over 4 s with its back to the room (its stance open), then the chime has taken one
	// more hammering (three and it breaks).
	if (St.HammerUntil > 0.0)
	{
		AWRCrystalBossProp* C = St.HammerOn.Get();
		if (Body) Body->Pose = 1;
		if (C)
		{
			const FVector To = (C->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
			Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
			if (T >= St.NextBlowAt)
			{
				St.NextBlowAt = T + CbT(Boss, TEXT("hammerSeconds"), 4.f) / FMath::Max(1.f, CbT(Boss, TEXT("hammerBlows"), 4.f));
				Boss->PlaySwing(EWRSwingKind::Heavy, 0.8f);
				C->TurnAt = C->ServerNow();
				C->ForceNetUpdate();
				WRSound::PlayAt(Boss, TEXT("crystal.boss.chime_hammered"), C->HitPoint());
				WRFx::Broadcast(Boss, TEXT("crystal.chime_ring"), C->HitPoint(), FRotator::ZeroRotator, 1.f);
			}
		}
		if (T >= St.HammerUntil || !C)
		{
			St.HammerUntil = 0.0;
			if (Body) Body->Pose = 0;
			if (C)
			{
				C->Value += 1.f;
				const int32 Need = FMath::RoundToInt(CbT(Boss, TEXT("chimeBreak"), 3.f));
				C->SetState(C->Value >= Need ? 2 : C->Value >= Need - 1 ? 1 : 0);
				if (C->State == 2) WRFx::Broadcast(Boss, TEXT("crystal.chime_break"), C->HitPoint(), FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.chime_break"));
			}
			St.HammerOn = nullptr;
			St.Heard.Reset();
			St.bGoal = false;
		}
		return true;
	}
	// The listening stance (2 s, horns sweeping): any noise in it is answered with the Snap lunge at that noise.
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id == TEXT("listening"))
	{
		if (Body) Body->Pose = 2;
		for (const FWRCrystalBossState::FNoise& N : St.Heard)
		{
			if (N.When <= St.ListenStart || N.Kind == TEXT("chime")) continue;
			St.bSnapDue = true;
			St.SnapAt = N.At;
			CancelAttack(Boss);
			break;
		}
		return false;
	}
	if (Body && Body->Pose == 2) Body->Pose = 0;
	if (Boss->Current != INDEX_NONE) return false;
	AWRCharacterBase* Nominal = PickTarget(Boss, Boss->PickBossTarget());
	// What it owes first: the Resonance blast after a blow that landed, the Snap lunge at a noise in its listening.
	if (St.bBlastDue && Nominal) { St.bBlastDue = false; if (Force(Boss, TEXT("resonance_blast"), Nominal)) return true; }
	if (St.bSnapDue && Nominal)
	{
		St.bSnapDue = false;
		St.Goal = St.SnapAt;
		St.bGoal = true;
		if (Force(Boss, TEXT("snap_lunge"), Nominal)) return true;
	}
	// The loudest noise within 30 m is where it goes.
	if (const FWRCrystalBossState::FNoise* N = CbLoudest(St))
	{
		St.Goal = N->At;
		St.bGoal = true;
		if (Body) Body->Point = N->At;
		// A struck chime: it goes to hammer it.
		if (N->Kind == TEXT("chime"))
		{
			AWRCrystalBossProp* Best = nullptr;
			for (const TWeakObjectPtr<AWRCrystalBossProp>& W : St.Chimes)
			{
				if (W.IsValid() && W->State < 2 && FVector::Dist(W->HitPoint(), N->At) < 400.f) Best = W.Get();
			}
			if (Best)
			{
				const FVector Stand = Best->GetActorLocation() + (St.Home - Best->GetActorLocation()).GetSafeNormal2D() * (Boss->GetSimpleCollisionRadius() + 160.f);
				if (FVector::Dist2D(Stand, Boss->GetActorLocation()) > 180.f)
				{
					Boss->AddMovementInput((Stand - Boss->GetActorLocation()).GetSafeNormal2D(), 1.f);
					return true;
				}
				HammerChime(Boss, St);
				St.HammerOn = Best;
				return true;
			}
		}
	}
	if (St.bGoal)
	{
		// Walk to the noise; close enough, its moves (the base machine) answer it there.
		const float D = float(FVector::Dist2D(St.Goal, Boss->GetActorLocation()));
		if (D > CbT(Boss, TEXT("slamReach"), 900.f))
		{
			Boss->AddMovementInput((St.Goal - Boss->GetActorLocation()).GetSafeNormal2D(), 1.f);
			return true;
		}
		return false;
	}
	// Heard nothing: it stands, horns sweeping, and every so often goes still to listen.
	if (Nominal && T >= St.ListenStart + CbT(Boss, TEXT("listenEvery"), 12.f))
	{
		Force(Boss, TEXT("listening"), Nominal);
		return true;
	}
	return true;
}

void FWRCrystalBoss::HammerChime(AWRBoss* Boss, FWRCrystalBossState& St)
{
	const double T = CbNow(Boss);
	St.HammerUntil = T + CbT(Boss, TEXT("hammerSeconds"), 4.f);
	St.NextBlowAt = T + 0.2;
	// Its back to the room the whole time: stance open (x1.5).
	Boss->StanceOpenUntil = St.HammerUntil;
	WRSound::PlayAt(Boss, TEXT("crystal.boss.warden_tell"), Boss->GetActorLocation());
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Echo Warden hammers a chime"));
}

// ================================================================================================ the Geode Tyrant

const TArray<FVector>& FWRCrystalBoss::TyrantRoute()
{
	// The tubes' own floors, walked in order (build-biome-crystal P2.2 4): the Wallow, out by B1-B4 to the Hall of Beams,
	// B1-B3 to the Chandelier Gallery and back, H6-B1 and J2-H6 up past the Guano Mines to the Organ, east along the
	// Organ to its end (J3) and back to J2, and up the throat (G6-J2) to the Agate Rinds.
	static TArray<FVector> Route;
	static bool bBuilt = false;
	if (bBuilt) return Route;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.bLoaded) return Route;
	bBuilt = true;
	const auto Hall = [&D](const TCHAR* Id) { const FWRCrystalData::FHall* H = D.FindHall(Id); return H ? H->At : FVector::ZeroVector; };
	const auto Add = [](const FVector& P) { if (!P.IsZero() && (Route.Num() == 0 || FVector::Dist2D(Route.Last(), P) > 600.f)) Route.Add(P); };
	const auto Tube = [&](const TCHAR* Id, bool bReverse, float FromX = -1.e9f, float ToX = 1.e9f)
	{
		const FWRCrystalData::FTube* Tb = D.FindTube(Id);
		if (!Tb) return;
		TArray<FVector> P;
		for (const FVector& V : Tb->Pts) if (V.X >= FromX && V.X <= ToX) P.Add(V + FVector(0.f, 0.f, 20.f));
		if (bReverse) Algo::Reverse(P);
		for (int32 i = 0; i < P.Num(); i += 4) Add(P[i]);
		if (P.Num()) Add(P.Last());
	};
	Add(Hall(TEXT("B4")));
	Tube(TEXT("B1-B4"), true);
	Add(Hall(TEXT("B1")));
	Tube(TEXT("B1-B3"), false);
	Add(Hall(TEXT("B3")));
	Tube(TEXT("B1-B3"), true);
	Add(Hall(TEXT("B1")));
	Tube(TEXT("H6-B1"), true);
	Add(Hall(TEXT("H6")));
	Tube(TEXT("J2-H6"), true);
	// The Organ east of J2 to J3 and back (its east end).
	if (const FWRCrystalData::FTube* Org = D.FindTube(TEXT("organ")))
	{
		const float JX = Hall(TEXT("J2")).IsZero() ? 2000.f * 100.f : Hall(TEXT("J2")).X;
		TArray<FVector> East;
		for (const FVector& V : Org->Pts) if (V.X >= JX - 200.f) East.Add(V + FVector(0.f, 0.f, 20.f));
		for (int32 i = 0; i < East.Num(); i += 6) Add(East[i]);
		for (int32 i = East.Num() - 1; i >= 0; i -= 6) Add(East[i]);
	}
	Tube(TEXT("G6-J2"), true);
	Add(Hall(TEXT("G6")));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Geode Tyrant's round: %d waypoints"), Route.Num());
	return Route;
}

namespace WRCrystalBossImpl
{
	/** Which plate row a point on the Tyrant is over: 0-2 the left flank front to back, 3-5 the right; -1 not on the back. */
	int32 CbRowAt(const AWRBoss* Boss, const FVector& P)
	{
		const FVector L = Boss->GetActorTransform().InverseTransformPositionNoScale(P);
		const float R = Boss->GetSimpleCollisionRadius();
		if (L.Z < -Boss->GetSimpleCollisionHalfHeight() * 0.2f) return -1;
		const int32 Section = L.X > R * 0.3f ? 0 : L.X > -R * 0.4f ? 1 : 2;
		return (L.Y < 0.f ? 0 : 3) + Section;
	}
}

bool FWRCrystalBoss::TickTyrant(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	// In a fight it hunts like a Prismback; its home follows it so a reset sends it on its round.
	(void)St; (void)Dt;
	return false;
}

// ================================================================================================ the attack hooks

bool FWRCrystalBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsCrystalBoss(Boss)) return true;
	const FWRCrystalBossState* St = CbStates().Find(Boss);
	const FName Id = Attack.Id;
	const float Dist = Target ? float(FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation())) : 0.f;
	if (Boss->CreatureId == TEXT("mirrored_twins"))
	{
		// Ward guard only answers shots: someone with a bow or a spell out at range.
		if (Id == TEXT("ward_guard")) return Target && Dist > 600.f && (Target->HasTag(WRTags::State_Casting) || Target->HasTag(WRTags::State_Charging) || Dist > 1200.f);
	}
	if (Boss->CreatureId == TEXT("echo_warden"))
	{
		if (Id == TEXT("resonance_blast") || Id == TEXT("snap_lunge") || Id == TEXT("chime_hammer")) return false;   // only forced
		if (Id == TEXT("fork_slam")) return St && St->bGoal;
	}
	if (Boss->CreatureId == TEXT("geode_tyrant"))
	{
		if (Id == TEXT("glare")) return CbLightNear(Boss->GetWorld(), Boss->GetActorLocation(), CbT(Boss, TEXT("glareLight"), 1500.f));
		if (Id == TEXT("shard_shrug")) return Dist >= 400.f;   // nothing lands inside 4 m
	}
	if (Boss->CreatureId == TEXT("lyrathis")) return LyrathisAllowed(Boss, Attack, Target);   // (WRCrystalLyrathis.cpp)
	return true;
}

void FWRCrystalBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	const FName Id = Attack.Id;
	if (St.Kind == TEXT("mirrored_twins"))
	{
		// The double winds up the same move from its side (its clip, its glint).
		if (AWRCreature* D = St.Double.Get())
		{
			D->PlaySwing(EWRSwingKind::Heavy, Attack.Windup + 0.2f, Id == TEXT("shield_bash") ? 1 : 0);
			D->MulticastTell(1, Id == TEXT("shield_bash") ? FName(TEXT("LeftHand")) : FName(TEXT("RightHand")), NAME_None);
			if (Target) D->SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - D->GetActorLocation()).Rotation().Yaw, 0.f));
		}
		St.DoubleHits.Reset();
		if (Id == TEXT("ward_guard"))
		{
			St.WardUntil = CbNow(Boss) + Attack.Windup + CbT(Boss, TEXT("wardSeconds"), 3.f);
			WRSound::PlayAt(Boss, TEXT("crystal.boss.ward_guard"), Boss->GetActorLocation());
		}
		else
		{
			WRSound::PlayAt(Boss, TEXT("crystal.boss.twins_tell"), Boss->GetActorLocation());
		}
	}
	if (St.Kind == TEXT("echo_warden"))
	{
		// Fork slam lands where the noise was, not on you; the Snap lunge runs at the noise.
		if ((Id == TEXT("fork_slam") || Id == TEXT("snap_lunge")) && St.bGoal)
		{
			FVector G;
			CbGround(Boss->GetWorld(), St.Goal, G, 300.f, 1200.f);
			Boss->AttackPoint = G;
			Boss->AttackFacing = FRotator(0.f, (St.Goal - Boss->GetActorLocation()).Rotation().Yaw, 0.f);
			Boss->SetActorRotation(Boss->AttackFacing);
		}
		if (Id == TEXT("listening")) St.ListenStart = CbNow(Boss);
		WRSound::PlayAt(Boss, Id == TEXT("listening") ? TEXT("crystal.boss.warden_listen") : TEXT("crystal.boss.warden_tell"), Boss->GetActorLocation());
	}
	if (St.Kind == TEXT("lyrathis")) LyrathisStarted(Boss, St, Attack, Target);
	if (St.Kind == TEXT("geode_tyrant"))
	{
		WRSound::PlayAt(Boss, Id == TEXT("glare") ? TEXT("crystal.boss.tyrant_glare") : Id == TEXT("shard_shrug") ? TEXT("crystal.boss.tyrant_shrug") : TEXT("crystal.boss.tyrant_tell"), Boss->GetActorLocation());
	}
	(void)Target;
}

bool FWRCrystalBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRCrystalBossState& St = CbS(Boss);
	const FName Id = Attack.Id;
	const double T = CbNow(Boss);
	if (St.Kind == TEXT("mirrored_twins"))
	{
		AWRCreature* D = St.Double.Get();
		St.bDoubleLine = false;
		if (D && Attack.Shape == EWRBossShape::Line)
		{
			// The double's own line: from its place, through where you stood, as far as the knight's (converging).
			const FVector Aim = Boss->AttackPoint + FVector(0.f, 0.f, D->GetSimpleCollisionHalfHeight());
			const FVector Dir = (Aim - D->GetActorLocation()).GetSafeNormal2D();
			St.DoubleFrom = D->GetActorLocation();
			St.DoubleTo = CbInside(St, D->GetActorLocation() + Dir * Attack.Range * 0.85f, 150.f);
			St.bDoubleLine = true;
		}
		if (Id == TEXT("ward_guard")) return true;   // nothing strikes: the shields reflect for 3 s
		return false;
	}
	if (St.Kind == TEXT("echo_warden"))
	{
		if (Id == TEXT("listening") || Id == TEXT("chime_hammer")) return true;   // still, listening / the hammering
		if (Id == TEXT("resonance_blast"))
		{
			// A visible ring of air rolling out from it (r 600 at 1,200 cm/s): low, jump it.
			WRFx::Broadcast(Boss, TEXT("crystal.resonance_ring"), CbFeet(Boss) + FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, Attack.Size / 100.f, TEXT("crystal.boss.resonance_blast"));
			Boss->StrikeEnds = T + Attack.Size / FMath::Max(100.f, CbT(Boss, TEXT("blastSpeed"), 1200.f));
			return true;
		}
		return false;
	}
	if (St.Kind == TEXT("lyrathis")) return LyrathisStrike(Boss, St, Attack);
	if (St.Kind == TEXT("geode_tyrant"))
	{
		if (Id == TEXT("glare"))
		{
			// The plates tilt and throw the brightest light near it into the eyes of everyone in the cone: a white-out
			// and a dropped lock (not Blind).
			const FVector Fwd = Boss->GetActorForwardVector().GetSafeNormal2D();
			for (AWRCharacterBase* C : CbFoes(Boss, Attack.Range, Boss->GetActorLocation()))
			{
				const FVector To = (C->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
				if (FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To, Fwd)), -1.f, 1.f))) > Attack.Size) continue;
				if (C->Status) C->Status->ApplyStatus(WRTags::Status_Glare, CbT(Boss, TEXT("glareSeconds"), 1.f));
			}
			WRFx::Broadcast(Boss, TEXT("crystal.glare"), Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), Fwd.Rotation(), 2.f, TEXT("crystal.boss.tyrant_glare"));
			return true;
		}
		if (Id == TEXT("shard_shrug"))
		{
			// Eight shards lobbed high to a ring round it; each landing spot glints the whole flight; never home.
			const int32 N = int32(CbT(Boss, TEXT("shards"), 8.f));
			const float Ring = CbT(Boss, TEXT("shardRing"), 800.f);
			const FVector From = Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.8f);
			for (int32 i = 0; i < N; ++i)
			{
				const float A = (i + FMath::FRand() * 0.6f) * 2.f * PI / N;
				FVector Land;
				CbGround(Boss->GetWorld(), Boss->GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::Max(450.f, Ring * FMath::FRandRange(0.7f, 1.f)), Land, 600.f, 1500.f);
				Lob(Boss, From, Land, CbT(Boss, TEXT("shardSpeed"), 900.f), Attack.Damage, CbT(Boss, TEXT("shardSplash"), 150.f), TEXT("shard"));
			}
			return true;
		}
		return false;
	}
	return false;
}

void FWRCrystalBoss::Lob(AWRBoss* Boss, const FVector& From, const FVector& To, float Speed, float Damage, float Splash, FName Kind)
{
	FActorSpawnParameters Params;
	Params.Owner = Boss;
	Params.Instigator = Boss;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Velocity = AWRProjectile::LobVelocity(From, To, Speed, 1.f);
	AWRProjectile* P = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Velocity.Rotation(), Params);
	if (!P) return;
	FWRHit Hit;
	Hit.Instigator = Boss;
	Hit.Damage = Damage * CbDamageScale();
	Hit.PoiseDamage = 60.f;
	Hit.Impact = 3;
	Hit.bHeavy = true;
	Hit.Tier = EWRHitTier::Heavy;
	Hit.Element = WRTags::Element_Physical;
	Hit.bReflectable = false;   // a lob is never thrown back (crystal 2.17)
	P->InitLob(Hit, Velocity, 1.f, Splash, FString());
	P->SetTrail(TEXT("crystal.shard_trail"));
	// The landing spot glints the whole flight (the shards' tell).
	const float Flight = FMath::Max(0.3f, float(FVector::Dist2D(From, To)) / FMath::Max(100.f, Speed));
	WRFx::Broadcast(Boss, TEXT("crystal.shard_glint"), To + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, Splash / 150.f, NAME_None, Flight);
	P->OnLanded = [WeakB = TWeakObjectPtr<AWRBoss>(Boss), Kind](const FVector& At)
	{
		if (AWRBoss* B = WeakB.Get()) WRFx::Broadcast(B, TEXT("crystal.shard_land"), At, FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.shard_land"));
	};
}

bool FWRCrystalBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRCrystalBossState& St = CbS(Boss);
	const FName Id = Attack.Id;
	const double T = CbNow(Boss);
	if (St.Kind == TEXT("mirrored_twins"))
	{
		if (Id == TEXT("ward_guard")) return true;
		DoubleStrike(Boss, St, Attack, DeltaSeconds);
		return false;   // the knight's own strike runs as usual
	}
	if (St.Kind == TEXT("echo_warden"))
	{
		if (Id == TEXT("listening") || Id == TEXT("chime_hammer")) return true;
		if (Id == TEXT("resonance_blast"))
		{
			// The ring's front: whoever it reaches on the ground takes it once (a jump clears it).
			const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
			const float Front = Attack.Size * FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
			for (AWRCharacterBase* C : CbFoes(Boss, Front, Boss->GetActorLocation()))
			{
				if (Boss->StrikeHits.Contains(C) || FVector::Dist2D(C->GetActorLocation(), Boss->GetActorLocation()) < Front - 120.f - C->GetSimpleCollisionRadius()) continue;
				Boss->StrikeHits.Add(C);
				const UCharacterMovementComponent* M = C->GetCharacterMovement();
				if (M && M->IsFalling()) continue;   // jumped it
				CbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, true, CbFeet(Boss), TEXT("mark"), Attack.bRow ? Attack.Row.Buildups.FindRef(TEXT("mark")) : 40.f, false);
			}
			return true;
		}
		return false;
	}
	if (St.Kind == TEXT("geode_tyrant"))
	{
		if (Id == TEXT("glare") || Id == TEXT("shard_shrug")) return true;
		if (Id == TEXT("plate_charge"))
		{
			// A selenite beam in its way (the Hall of Beams, the Chandelier Gallery): the beam shatters and it's stunned.
			const FWRCrystalData& D = FWRCrystalData::Get();
			const FVector Nose = Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 80.f);
			for (const FWRCrystalData::FBeam& Bm : D.Beams)
			{
				const FVector P = FMath::ClosestPointOnSegment(Nose, Bm.A, Bm.B);
				if (FVector::Dist(P, Nose) > Bm.Thick * 0.5f + 120.f) continue;
				WRFx::Broadcast(Boss, TEXT("crystal.beam_shatter"), P, FRotator::ZeroRotator, 2.f, TEXT("crystal.boss.beam_shatter"));
				CancelAttack(Boss);
				if (Boss->Status) Boss->Status->Stagger(CbT(Boss, TEXT("beamStun"), 3.f));
				Boss->StanceOpenUntil = T + CbT(Boss, TEXT("beamStun"), 3.f);
				for (AWRCharacterBase* C : CbFoes(Boss, 2500.f, P)) CbTell(C, TEXT("It charged into a selenite beam: the beam shatters over it"));
				UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Geode Tyrant charges into a selenite beam"));
				return true;
			}
		}
		return false;
	}
	if (St.Kind == TEXT("lyrathis")) return false;   // (WRCrystalLyrathis.cpp hooks her strikes through Tick)
	return false;
}

void FWRCrystalBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	if (St.Kind == TEXT("mirrored_twins")) St.bDoubleLine = false;
	if (St.Kind == TEXT("lyrathis") && Attack.Id == TEXT("sundering_flare")) if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Pose = 0;
	if (St.Kind == TEXT("echo_warden") && Attack.Id == TEXT("listening"))
	{
		St.ListenStart = CbNow(Boss);
		if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Pose = 0;
	}
}

void FWRCrystalBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (!IsCrystalBoss(Boss)) return;
	// The Crystal caves' element on its blows that carry mark (the Resonance blast, Lyrathis's beams).
	if (Attack.bRow && Attack.Row.Buildups.Contains(TEXT("mark"))) Hit.Element = WRTags::Element_Arcane;
}

void FWRCrystalBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRCrystalBossState& St = CbS(Boss);
	// The Warden: after any of its blows lands, the Resonance blast rings out (its tines blur 1 s first).
	if (St.Kind == TEXT("lyrathis")) LyrathisLanded(Boss, St, Victim, AttackId, Result);
	if (St.Kind == TEXT("echo_warden") && AttackId != TEXT("resonance_blast") && (Result == EWRHitResult::Hit || Result == EWRHitResult::Blocked || Result == EWRHitResult::GuardBroken))
	{
		St.bBlastDue = true;
	}
	(void)Victim;
}

float FWRCrystalBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority() || Damage <= 0.f) return Damage;
	FWRCrystalBossState& St = CbS(Boss);
	const double T = CbNow(Boss);
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	if (St.Kind == TEXT("mirrored_twins"))
	{
		// The real knight takes nothing while the lens flares (the swap); its Ward guard throws shots back.
		if (St.SwapStart >= 0.0) return 0.f;
		if (T < St.WardUntil && WRArcane::IsReflectable(Hit) && By)
		{
			WRArcane::Reflect(Boss, Hit, Damage, CbT(Boss, TEXT("wardReflect"), 0.8f), CbHitFrom(Boss, Hit));
			return 0.f;
		}
		return Damage;
	}
	if (St.Kind == TEXT("geode_tyrant"))
	{
		const FVector At = CbHitFrom(Boss, Hit);
		const int32 Row = CbRowAt(Boss, At);
		UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
		const bool bWhole = Row >= 0 && !(Body && (Body->Broken & (1 << Row)));
		// The Void passes the plates (crystal 2.13.3).
		if (Hit.Element == WRTags::Element_Void) return Damage;
		if (Hit.bProjectile && !Hit.bMelee)
		{
			// A shot from a side whose row is whole is thrown back (Mana burn spells crack the row instead, x2).
			const FVector From = (Hit.Origin.IsZero() ? At : Hit.Origin) - Boss->GetActorLocation();
			const int32 SideRow = (Boss->GetActorTransform().InverseTransformVectorNoScale(From).Y < 0.f ? 0 : 3) + 1;
			const bool bSideWhole = !(Body && ((Body->Broken >> (SideRow - 1)) & 7) == 7);
			if (bSideWhole)
			{
				if (WRArcane::BurnsShields(By, Hit))
				{
					const int32 R = SideRow + (Row >= 0 ? (Row % 3) - 1 : 0);
					St.RowHP[FMath::Clamp(R, 0, 5)] -= Damage * WRTune(TEXT("arcane.manaBurnShield"), 2.f);
				}
				else if (WRArcane::IsReflectable(Hit) && By)
				{
					WRArcane::Reflect(Boss, Hit, Damage, CbT(Boss, TEXT("reflect"), 0.8f), At);
					return 0.f;
				}
			}
		}
		else if (Hit.bMelee && bWhole)
		{
			// A blow on a plate row cracks it (strike best); the body takes the blow as well.
			const float Mult = Hit.PhysType == 2 ? CbT(Boss, TEXT("rowStrike"), 1.4f) : Hit.PhysType == 1 ? CbT(Boss, TEXT("rowSlash"), 0.7f) : Hit.PhysType == 3 ? CbT(Boss, TEXT("rowPierce"), 0.8f) : 1.f;
			St.RowHP[Row] -= Damage * Mult;
		}
		for (int32 r = 0; r < 6; ++r)
		{
			if (St.RowHP[r] > 0.f || !Body || (Body->Broken & (1 << r))) continue;
			Body->Broken |= (1 << r);
			WRFx::Broadcast(Boss, TEXT("crystal.plate_break"), At, FRotator::ZeroRotator, 2.f, TEXT("crystal.boss.plate_break"));
			if (By) CbTell(By, TEXT("A plate row splits off its back: that flank stops throwing shots back"));
			UE_LOG(LogWildreach, Log, TEXT("CRYSTAL the Geode Tyrant loses plate row %d"), r);
		}
		return Damage;
	}
	if (St.Kind == TEXT("lyrathis")) return LyrathisIncoming(Boss, St, Hit, Damage);
	if (St.Kind == TEXT("echo_warden") && St.HammerUntil > T && By)
	{
		// From behind while it hammers: its back is open (stance x1.5 is StanceOpenUntil; the blow lands normally).
		return Damage;
	}
	return Damage;
}

bool FWRCrystalBoss::OwnWeakness(const AWRBoss* Boss)
{
	return IsCrystalBoss(Boss);
}

void FWRCrystalBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	(void)Boss; (void)Hit; (void)Damage;
}

void FWRCrystalBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsCrystalBoss(Boss) || !Boss->HasAuthority() || Boss->CreatureId != TEXT("lyrathis")) return;
	FWRCrystalBossState& St = CbS(Boss);
	// Phase 2: she shatters into herself and her copies (StepCopies stands them on the prisms' lines). Phase 3: to the
	// dais; every spire on the surface blazes.
	if (NewPhase >= 2 && St.bDome && !St.bDomePermanent) BreakDome(Boss, St, TEXT("time"));
	for (AWRCharacterBase* C : CbFoes(Boss, St.ArenaR + 500.f, St.Home)) CbTell(C, NewPhase == 2 ? TEXT("She shatters into light: her copies stand on the prisms' lines. Turn a prism into a wall to put one out") : TEXT("She takes the dais: every prism fires round her"));
	if (NewPhase >= 3) St.P3NextDomeAt = 0.0;
}

bool FWRCrystalBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!IsCrystalBoss(Boss)) return false;
	if (Boss->CreatureId == TEXT("geode_tyrant")) return true;   // it roams: no stone (as the island's roamers)
	const FWRCrystalData& D = FWRCrystalData::Get();
	bool bAny = false;
	for (const FWRCrystalData::FRunback& R : D.Runback)
	{
		if (R.Boss != Boss->CreatureId) continue;
		bAny = true;
		bool bThere = false;
		for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
		{
			if (It->BossId == Boss->CreatureId && FVector::Dist(It->GetActorLocation(), R.At) < 800.f) { bThere = true; if (!Boss->Runback.IsValid()) Boss->Runback = *It; }
		}
		if (bThere) continue;
		FVector G;
		if (!CbGround(Boss->GetWorld(), R.At, G, 400.f, 2000.f)) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Face(0.f, (Boss->GetHome() - G).Rotation().Yaw, 0.f);
		if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), G, Face, Params))
		{
			Stone->BossId = Boss->CreatureId;
			if (!Boss->Runback.IsValid()) Boss->Runback = Stone;
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s shrine-stone (%s door) at %s"), *Boss->GetDisplayLabel(), *R.Door.ToString(), *G.ToCompactString());
		}
	}
	return bAny;
}

FName FWRCrystalBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == TEXT("lyrathis")) return bScarred ? FName(TEXT("part_prism_heart_scarred")) : FName(TEXT("part_prism_heart"));
	return Fallback;
}

bool FWRCrystalBoss::HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bProjectile)
{
	AWRCrystalBossProp* P = Cast<AWRCrystalBossProp>(Thing);
	if (!P || !P->HasAuthority()) return false;
	(void)Comp;
	return P->TakeHit(By, Damage, bProjectile, P->HitPoint());
}

// ================================================================================================ the machines (server)

bool AWRCrystalBossProp::TakeHit(AWRCharacterBase* By, float Damage, bool bProjectile, const FVector& At)
{
	if (!HasAuthority()) return false;
	const float Now = ServerNow();
	AWRBoss* Boss = OwnerBoss.Get();
	if (Kind == TEXT("lens"))
	{
		// Only while it flares (the swap's 1 s) is the lens out of its yoke's shadow: 3 shots or spells shatter it.
		if (State != 0 || !Boss) return true;
		FWRCrystalBossState* St = CbStates().Find(Boss);
		if (!St || !bProjectile || Now - TurnAt > CbT(Boss, TEXT("flareSeconds"), 1.f) + 0.1f)
		{
			WRSound::PlayAt(this, TEXT("crystal.boss.lens_tick"), At);
			return true;
		}
		++Hits;
		WRFx::Broadcast(this, TEXT("crystal.lens_crack"), At, FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.lens_crack"));
		if (Hits >= FMath::RoundToInt(CbT(Boss, TEXT("lensHits"), 3.f))) FWRCrystalBoss::ShatterLens(Boss, *St);
		return true;
	}
	if (Kind == TEXT("chime"))
	{
		// Any blow or shot rings it (a 25 m noise: the loudest thing in the hall).
		if (State >= 2) return true;
		TurnAt = Now;
		ForceNetUpdate();
		WRSound::PlayAt(this, TEXT("crystal.boss.chime_struck"), At);
		WRCrystal::Emit(this, HitPoint(), WRCrystal::NoiseRadius(TEXT("chime")), this, TEXT("chime"));
		return true;
	}
	if (Kind == TEXT("turntable"))
	{
		// Strike the hand-plate: the lever swings, the pinion turns the ring gear and the disc goes round exactly 45
		// degrees over 0.8 s (clockwise from above: the pawl allows no other way), the pawl dropping home with a clack.
		if (Now < ReadyAt) return true;
		Clicks += 1;
		TurnAt = Now;
		ReadyAt = Now + WRTune(TEXT("bosses.lyrathis.turnSeconds"), 0.8f);
		ForceNetUpdate();
		WRSound::PlayAt(this, TEXT("crystal.boss.turntable"), At);
		return true;
	}
	if (Kind == TEXT("lance"))
	{
		if (++Hits >= int32(WRTune(TEXT("bosses.lyrathis.lanceHits"), 3.f)))
		{
			WRFx::Broadcast(this, TEXT("crystal.lens_shatter"), GetActorLocation(), FRotator::ZeroRotator, 0.8f, TEXT("crystal.boss.lens_shatter"));
			Destroy();
		}
		else WRFx::Broadcast(this, TEXT("crystal.lens_crack"), At, FRotator::ZeroRotator, 0.7f, TEXT("crystal.boss.lens_crack"));
		return true;
	}
	(void)By; (void)Damage;
	return false;
}

// ================================================================================================ UWRCrystalArenas

bool UWRCrystalArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRCrystalArenas::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && WRCrystal::Live() && FWRCrystalData::Get().bLoaded;
}

void UWRCrystalArenas::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	PlaceClock += DeltaTime;
	if (PlaceClock < 2.f) return;
	PlaceClock = 0.f;
	const UWRCrystalWorld* W = UWRCrystalWorld::Get(World);
	if (!W || !W->IsBuilt()) return;
	static const FName Ids[] = { TEXT("mirrored_twins"), TEXT("echo_warden"), TEXT("geode_tyrant"), TEXT("lyrathis") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		if (Place(Id)) Placed.Add(Id);
	}
}

bool UWRCrystalArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	FVector Centre;
	float Radius = 0.f, Yaw = 0.f;
	if (!FWRCrystalBoss::ArenaOf(BossId, Centre, Radius, Yaw) || !FWRCreatureRegistry::Get().Find(BossId)) return true;   // nothing to place
	// Lyrathis comes with her hall's machines (WRCrystalLyrathis.cpp); she isn't placed until she's built.
	if (BossId == TEXT("lyrathis") && WRTune(TEXT("bosses.lyrathis.live"), 0.f) <= 0.f) return true;
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Centre) < WRTune(TEXT("bosses.crystalPlaceRadius"), 30000.f);
	if (!bNear) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It) if (It->CreatureId == BossId) return true;
	FVector G;
	if (!CbGround(World, Centre + FVector(0.f, 0.f, 100.f), G, 300.f, 1500.f)) return false;
	const FWRCrystalData::FHall* Hall = nullptr;
	for (const FWRCrystalData::FHall& H : FWRCrystalData::Get().Halls) if (H.Arena == BossId || H.Lair == BossId) Hall = &H;
	const FRotator R(0.f, Yaw, 0.f);
	const auto SpawnProp = [World](FName Kind, const FVector& At, float PropYaw, float Size, int32 Index) -> AWRCrystalBossProp*
	{
		const FTransform Tr(FRotator(0.f, PropYaw, 0.f), At);
		AWRCrystalBossProp* A = World->SpawnActorDeferred<AWRCrystalBossProp>(AWRCrystalBossProp::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Kind = Kind;
			A->Size = Size;
			A->Index = Index;
			A->FinishSpawning(Tr);
		}
		return A;
	};
	if (BossId == TEXT("mirrored_twins") && Hall)
	{
		// The Spar Lens hangs in the gimbal under the light vent in the ceiling's middle; the magazine stands on the north
		// wall with its chain to the ceiling pulley and the counterweight's slot beside it.
		SpawnProp(TEXT("lens"), FVector(G.X, G.Y, G.Z + Hall->H - 40.f), Yaw, Hall->H, 0);
		const FVector North = G + R.RotateVector(FVector(0.f, -Hall->Ry + 30.f, 0.f));
		if (AWRCrystalBossProp* M = SpawnProp(TEXT("magazine"), North, Yaw, Hall->H, 0)) { M->Value = WRTune(TEXT("bosses.mirrored_twins.spares"), 3.f); }
	}
	if (BossId == TEXT("echo_warden") && Hall)
	{
		// The four chime stones at the compass points (where the hall's dressing left their stone heaps).
		for (int32 c = 0; c < 4; ++c)
		{
			const float Ang = c * 90.f;
			const FVector P = G + FVector(FMath::Cos(FMath::DegreesToRadians(Ang)) * Hall->Rx * 0.62f, FMath::Sin(FMath::DegreesToRadians(Ang)) * Hall->Ry * 0.62f, 0.f);
			FVector PG;
			CbGround(World, P + FVector(0.f, 0.f, 200.f), PG, 300.f, 1000.f);
			SpawnProp(TEXT("chime"), PG, Ang + 180.f, 300.f, c);
		}
	}
	if (BossId == TEXT("lyrathis") && Hall)
	{
		// The eight prisms on their turntables 22 m round the dais, each facing out along its own light shaft (index x 45
		// + 22.5, the Prism Crown's spires above), the lever and hand-plate outboard so nothing crosses the prism; the
		// Arcane altar on the dais's west edge (the dressing's carved basin is its look: the altar actor is hidden).
		for (int32 i = 0; i < 8; ++i)
		{
			const float Ang = i * 45.f + 22.5f;
			const FVector P = G + FRotator(0.f, Ang, 0.f).Vector() * WRTune(TEXT("bosses.lyrathis.ringRadius"), 2200.f);
			FVector PG;
			CbGround(World, P + FVector(0.f, 0.f, 300.f), PG, 400.f, 1500.f);
			SpawnProp(TEXT("turntable"), PG, Ang, WRTune(TEXT("bosses.lyrathis.prismHeight"), 600.f), i);
		}
		const FTransform AT(FRotator(0.f, Yaw, 0.f), G + R.RotateVector(FVector(-500.f, 0.f, 0.f)) + FVector(0.f, 0.f, 70.f));
		if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), AT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Altar->PathId = TEXT("arcane");
			Altar->FinishSpawning(AT);
			Altar->SetActorHiddenInGame(true);
		}
	}
	FVector Start = G;
	if (BossId == TEXT("lyrathis")) Start = G + FVector(0.f, 0.f, 66.f);   // on the dais
	if (BossId == TEXT("echo_warden") && Hall)
	{
		// It stands before the Vault Door, facing into its hall.
		Start = G + R.RotateVector(FVector(-Hall->Rx * 0.45f, 0.f, 0.f));
		CbGround(World, Start + FVector(0.f, 0.f, 200.f), Start, 300.f, 1000.f);
	}
	const FTransform Where(FRotator(0.f, Yaw + 180.f, 0.f), Start + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = false;   // the caves' floors, not the ground above them
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}
