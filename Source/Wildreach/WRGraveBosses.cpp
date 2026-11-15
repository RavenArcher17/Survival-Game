// The Graveyard's bosses: their counter mechanics, own moves and the hooks AWRBoss calls (cooks/biome-graveyard.md 2.13,
// 2.14; cooks/build-biome-graveyard.md 2.15, 2.16). See WRGraveBosses.h. The arena pieces' and the bodies' looks are
// WRGraveBossLook.cpp; placement and the held actions WRGraveArenas.cpp.
//
// Every blow here goes through UWRCombatLibrary::ApplyHit, and only where the body (or the thing it threw) really is:
// lobs fly a gravity arc from a replicated launch and never home, nothing lands inside 4 m, the Keen stops at a pillar.
// Unblockable moves wind up 0.9 s or more with a crimson glint (their rows' tells).

#include "WRGraveBosses.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRGraveCreatures.h"
#include "WRGraveTypes.h"
#include "WRGraveWorld.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

/** A survivor (or tame) held by a Graveyard boss: the Knell, the palps, the ribcage, the cage, the Shroud. */
struct FWRGraveBossHold
{
	TWeakObjectPtr<AWRCharacterBase> Who;
	FName How;
	double Start = 0.0, Ends = 0.0, TickAt = 0.0;
	float Dps = 0.f;
	/** What allies have dealt toward freeing them (the palp, the ribs, the cage door / his back). */
	float Free = 0.f;
	int32 Rings = 0;
};

/** Held in the ground: buried by the Gravedigger, a tame half dragged under by Lay to rest. */
struct FWRGraveBuried
{
	TWeakObjectPtr<AWRCharacterBase> Who;
	FName How;
	double Until = 0.0, TickAt = 0.0;
	TWeakObjectPtr<AWRGraveBossProp> Mound;
};

/** Server: one Graveyard boss's fight. */
struct FWRGraveBossState
{
	FName Kind;
	bool bBegun = false;
	FVector Home = FVector::ZeroVector;
	float ArenaR = 3000.f, Yaw = 0.f;
	FWRGraveBossHold Hold;
	TArray<FWRGraveBuried> Buried;
	TArray<TWeakObjectPtr<AWRCreature>> Adds;
	TWeakObjectPtr<AActor> LastBy;

	// The Bell-Ringer.
	TWeakObjectPtr<AWRGraveBossProp> Belfry;
	double NextTollAt = 0.0;
	bool bToRope = false;

	// The Crypt Mother.
	TArray<TWeakObjectPtr<AWRGraveBossProp>> Sacs;
	TArray<double> SacTorch;
	double NextHatchAt = 0.0, FoundUntil = 0.0;
	float CeilingZ = 0.f;
	bool bClimbing = false, bDropping = false;
	FVector ClimbFrom = FVector::ZeroVector;

	// The Ossuary Lord.
	bool bPile = false, bFinal = false, bChandelierUsed = false;
	double PileUntil = 0.0;
	int32 Lime = 0;
	float Keystone[3] = { 0.f, 0.f, 0.f };
	FVector PileAt = FVector::ZeroVector;
	TWeakObjectPtr<AWRGraveBossProp> Chandelier;
	FVector SurgeFrom = FVector::ZeroVector, SurgeTo = FVector::ZeroVector;
	bool bSurging = false;
	TSet<TWeakObjectPtr<AActor>> SurgeHit;

	// The Gravedigger.
	int32 Route = -1, StopIndex = 0;
	TArray<int32> StopsAt;
	double DigUntil = 0.0;
	bool bOnRound = false;
	float LanternHP = 150.f, DoorHP = 300.f;
	double BlindUntil = 0.0;
	FVector LastSeen = FVector::ZeroVector;
	bool bSpareUsed = false;
	TArray<TWeakObjectPtr<AWRCreature>> Hounds;
	double NextHoundStep = 0.0;

	// Nhal.
	TArray<TWeakObjectPtr<AWRGraveBossProp>> Tombs;
	double NextRaiseAt = 0.0;
	struct FPool { FVector At = FVector::ZeroVector; double Until = 0.0, TickAt = 0.0; };
	TArray<FPool> Pools;
	TArray<FVector> Hands;
	TWeakObjectPtr<AWRGraveBossProp> Procession;
	double NextShadeToll = 0.0;
	TMap<TWeakObjectPtr<AActor>, double> ShadeHitAt;
	TMap<FName, float> BaseWindup;
	TMap<FName, float> BaseHold;
	double KneelUntil = 0.0;
	bool bKeenSecond = false;
	/** Phase 2 has begun and The Opening hasn't been made yet (it comes as soon as she's free). */
	bool bOpeningDue = false;
};

namespace WRGraveBossImpl
{
	TMap<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& GbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState> Map;
		return Map;
	}
	FWRGraveBossState& GbS(const AWRBoss* Boss) { return GbStates().FindOrAdd(Boss); }
	FWRGraveBossState* GbFind(const AWRBoss* Boss) { return GbStates().Find(Boss); }
	double GbNow(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }
	float GbDamageScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }
	float GbT(const AWRBoss* Boss, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback); }
	FName GbKindOf(const AWRBoss* Boss)
	{
		if (!Boss) return NAME_None;
		const FName Id = Boss->CreatureId;
		return Id == TEXT("bell_ringer") || Id == TEXT("crypt_mother") || Id == TEXT("ossuary_lord") || Id == TEXT("gravedigger") || Id == TEXT("nhal") ? Id : NAME_None;
	}
	FVector GbFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }
	bool GbGround(const UWorld* World, const FVector& At, FVector& Out, float Up = 1500.f, float Down = 4000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGraveBossGround), false);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}
	TArray<AWRCharacterBase*> GbFoes(const AWRBoss* Boss, float Range, const FVector& From)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It)) continue;
			if (FVector::Dist2D(It->GetActorLocation(), From) > Range + It->GetSimpleCollisionRadius()) continue;
			if (FMath::Abs(It->GetActorLocation().Z - From.Z) > 1500.f) continue;
			Out.Add(*It);
		}
		return Out;
	}
	bool GbSurvivorSide(const AActor* A)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(A);
		return C && (C->GetPlayerState() || !C->GetTameOwner().IsEmpty());
	}
	/** A blow that isn't its row's own (a ring, a hand, a ring of shades), with the boss's scaling. */
	void GbHitWith(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, bool bUnblockable, bool bLow, const FVector& Origin, FName Buildup = NAME_None, float Amount = 0.f, float Stamina = -1.f)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * GbDamageScale() * (Boss->Phase >= 3 ? GbT(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = Poise;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bHeavy = Damage >= 30.f;
		Hit.Tier = Damage >= 30.f ? EWRHitTier::Heavy : EWRHitTier::Light;
		Hit.Origin = Origin;
		Hit.Element = WRTags::Element_Physical;
		Hit.PhysType = 2;
		Hit.StaminaDamage = Stamina;
		if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	/** A held body's squeeze: damage over time through any guard (no flinch); a little stamina too (the Shroud). */
	void GbSqueeze(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, FName Buildup = NAME_None, float Amount = 0.f, float Stamina = -1.f)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * GbDamageScale();
		Hit.bUnblockable = true;
		Hit.bDamageOverTime = true;
		Hit.Origin = Boss->GetActorLocation();
		Hit.Element = WRTags::Element_Physical;
		Hit.StaminaDamage = Stamina;
		if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	float GbHealthFraction(const AWRBoss* Boss)
	{
		const UWRAttributeSet* A = Boss->GetAttributes();
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}
	void GbSetHealth(AWRBoss* Boss, float Fraction)
	{
		UWRAttributeSet* A = Boss->GetAttributes();
		UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
		if (A && ASC) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Max(1.f, A->GetMaxHealth() * Fraction));
	}
	AWRCharacterBase* GbFurthest(const AWRBoss* Boss, float Range)
	{
		AWRCharacterBase* Best = nullptr;
		float BestD = -1.f;
		for (AWRCharacterBase* C : GbFoes(Boss, Range, Boss->GetActorLocation()))
		{
			if (!C->GetPlayerState()) continue;
			const float D = float(FVector::Dist2D(C->GetActorLocation(), Boss->GetActorLocation()));
			if (D > BestD) { BestD = D; Best = C; }
		}
		return Best;
	}
	AWRCreature* GbFurthestTame(const AWRBoss* Boss, float Range)
	{
		AWRCreature* Best = nullptr;
		float BestD = -1.f;
		for (AWRCharacterBase* C : GbFoes(Boss, Range, Boss->GetActorLocation()))
		{
			AWRCreature* Cr = Cast<AWRCreature>(C);
			if (!Cr || !Cr->IsTamed()) continue;
			const float D = float(FVector::Dist2D(C->GetActorLocation(), Boss->GetActorLocation()));
			if (D > BestD) { BestD = D; Best = Cr; }
		}
		return Best;
	}
	/** The survivor who owns a tame (by name), or null. */
	AActor* GbOwnerOf(const AWRCreature* Tame)
	{
		if (!Tame || Tame->GetTameOwner().IsEmpty()) return nullptr;
		for (FConstPlayerControllerIterator It = Tame->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Tame->GetTameOwner()) return PC->GetPawn();
		}
		return nullptr;
	}
	void GbTell(AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** Where a blow met the body: the impact point, else its origin, else the striker. */
	FVector GbHitFrom(const AWRBoss* Boss, const FWRHit& Hit)
	{
		if (!Hit.ImpactPoint.IsZero()) return Hit.ImpactPoint;
		if (!Hit.Origin.IsZero()) return Hit.Origin;
		return Hit.Instigator ? Hit.Instigator->GetActorLocation() : Boss->GetActorLocation();
	}
	/** A world point on the boss's body (a fraction of its radius forward / right, of its half height up). */
	FVector GbBodyPoint(const AWRBoss* Boss, float Fwd, float Right, float Up)
	{
		const FRotator Y(0.f, Boss->GetActorRotation().Yaw, 0.f);
		const float R = Boss->GetSimpleCollisionRadius(), H = Boss->GetSimpleCollisionHalfHeight();
		return Boss->GetActorLocation() + Y.Vector() * (R * Fwd) + FRotationMatrix(Y).GetUnitAxis(EAxis::Y) * (R * Right) + FVector(0.f, 0.f, H * Up);
	}
	/** The Gravedigger's lantern (on its pole off the cage, up behind his left shoulder) and his back's cage. */
	FVector GbLanternAt(const AWRBoss* Boss) { return GbBodyPoint(Boss, -1.2f, -0.6f, 1.25f); }
	FVector GbCageAt(const AWRBoss* Boss) { return GbBodyPoint(Boss, -1.1f, 0.f, 0.45f); }
	/** The Ossuary Lord's three keystones in its pile (brow, chest, spine), spread across the bones. */
	FVector GbKeystoneAt(const FWRGraveBossState& St, int32 i)
	{
		return St.PileAt + FRotator(0.f, St.Yaw + 120.f * i, 0.f).Vector() * 160.f + FVector(0.f, 0.f, 40.f);
	}
	bool GbBloodMoon() { return WRGraveSky::BloodMoon > 0.5f; }
	/** The Belfry's tower base (world), from its prop or the site. */
	FVector GbRopeFoot(const FWRGraveBossState& St)
	{
		if (const AWRGraveBossProp* B = St.Belfry.Get()) return B->GetActorTransform().TransformPosition(WRGraveBelfry::RopeFoot());
		return St.Home;
	}
	int32 GbLive(TArray<TWeakObjectPtr<AWRCreature>>& List)
	{
		List.RemoveAll([](const TWeakObjectPtr<AWRCreature>& W) { return !W.IsValid() || !W->IsAlive(); });
		return List.Num();
	}
	bool GbIsNature(const FWRHit& Hit) { return Hit.PathId == TEXT("nature"); }
	bool GbIsFire(const FWRHit& Hit)
	{
		return Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f || Hit.Buildups.FindRef(TEXT("burn")) > 0.f
			|| (Hit.bMelee && UWRSurvivalComponent::HoldsTorch(Hit.Instigator.Get()));
	}
	void GbMoveTo(AWRBoss* Boss, const FVector& Goal, float Input = 1.f)
	{
		const FVector To = Goal - Boss->GetActorLocation();
		if (To.Size2D() < 60.f) return;
		Boss->AddMovementInput(To.GetSafeNormal2D(), Input);
	}
}
using namespace WRGraveBossImpl;

// ================================================================================================ the hooks

bool FWRGraveBoss::IsGraveBoss(const AWRBoss* Boss)
{
	return !GbKindOf(Boss).IsNone();
}

TArray<TWeakObjectPtr<AWRGraveBossProp>>& FWRGraveBoss::Props()
{
	static TArray<TWeakObjectPtr<AWRGraveBossProp>> List;
	List.RemoveAll([](const TWeakObjectPtr<AWRGraveBossProp>& P) { return !P.IsValid(); });
	return List;
}

void FWRGraveBoss::Begin(AWRBoss* Boss)
{
	if (!IsGraveBoss(Boss)) return;
	UWRGraveBossBody::Attach(Boss);
	if (!Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	St.Kind = GbKindOf(Boss);
	St.bBegun = true;
	if (!ArenaOf(St.Kind, St.Home, St.ArenaR, St.Yaw))
	{
		St.Home = Boss->GetActorLocation();
	}
	const double T = GbNow(Boss);
	for (const TWeakObjectPtr<AWRGraveBossProp>& W : Props())
	{
		AWRGraveBossProp* P = W.Get();
		if (!P || FVector::Dist2D(P->GetActorLocation(), St.Home) > St.ArenaR + 3000.f) continue;
		if (St.Kind == TEXT("bell_ringer") && P->Kind == TEXT("belfry")) St.Belfry = P;
		if (St.Kind == TEXT("crypt_mother") && P->Kind == TEXT("sac")) St.Sacs.Add(P);
		if (St.Kind == TEXT("ossuary_lord") && P->Kind == TEXT("chandelier")) St.Chandelier = P;
		if (St.Kind == TEXT("nhal") && P->Kind == TEXT("tomb")) St.Tombs.Add(P);
		P->OwnerBoss = Boss;
		// A new boss (its spawner brought it back): its arena's pieces are whole again.
		if (P->Kind == TEXT("belfry") || P->Kind == TEXT("sac") || P->Kind == TEXT("chandelier") || P->Kind == TEXT("tomb"))
		{
			P->Health = P->MaxHealth;
			P->SetState(0);
		}
	}
	St.SacTorch.Init(0.0, St.Sacs.Num());
	St.Tombs.Sort([](const TWeakObjectPtr<AWRGraveBossProp>& A, const TWeakObjectPtr<AWRGraveBossProp>& B) { return A.IsValid() && B.IsValid() && A->Index < B->Index; });
	for (const FWRBossAttack& A : Boss->Attacks)
	{
		St.BaseWindup.Add(A.Id, A.Windup);
		St.BaseHold.Add(A.Id, A.bRow ? A.Row.Hold : 0.f);
	}
	St.NextTollAt = T + 8.0;
	St.NextHatchAt = T + GbT(Boss, TEXT("hatchEvery"), 30.f);
	St.NextRaiseAt = T + GbT(Boss, TEXT("raiseEvery"), 25.f);
	St.LanternHP = GbT(Boss, TEXT("lanternHealth"), 150.f);
	St.DoorHP = GbT(Boss, TEXT("doorHealth"), 300.f);
	for (int32 k = 0; k < 3; ++k) St.Keystone[k] = GbT(Boss, TEXT("keystoneHealth"), 120.f);
	if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss))
	{
		Body->Count = St.Kind == TEXT("crypt_mother") ? uint8(GbT(Boss, TEXT("riders"), 20.f)) : 0;
	}
	// The Bell-Ringer keeps to the rope; the others to their hall's floor.
	if (St.Kind == TEXT("bell_ringer")) Boss->SetHome(GbRopeFoot(St) + FVector(300.f, 0.f, 0.f));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE boss %s begins (arena %s r %.0f; %d sacs, %d tombs)"), *Boss->GetDisplayLabel(), *St.Home.ToCompactString(), St.ArenaR, St.Sacs.Num(), St.Tombs.Num());
}

void FWRGraveBoss::Reset(AWRBoss* Boss)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	ReleaseGrab(Boss, St, false);
	for (FWRGraveBuried& B : St.Buried) { if (AWRCharacterBase* W = B.Who.Get()) Freed(W); }
	St.Buried.Reset();
	for (const TWeakObjectPtr<AWRCreature>& A : St.Adds) if (A.IsValid()) A->Destroy();
	St.Adds.Reset();
	St.Pools.Reset();
	St.Hands.Reset();
	const double T = GbNow(Boss);
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	if (Body)
	{
		Body->Pose = 0;
		Body->bBroken = false;
		Body->Veil = 0;
		Body->Caged = nullptr;
		Body->Count = St.Kind == TEXT("crypt_mother") ? uint8(GbT(Boss, TEXT("riders"), 20.f)) : 0;
	}
	if (AWRGraveBossProp* B = St.Belfry.Get()) { B->Health = B->MaxHealth; B->SetState(0); }
	for (const TWeakObjectPtr<AWRGraveBossProp>& S : St.Sacs) if (S.IsValid()) { S->Health = S->MaxHealth; S->SetState(0); }
	for (double& D : St.SacTorch) D = 0.0;
	if (AWRGraveBossProp* C = St.Chandelier.Get()) { C->Health = C->MaxHealth; C->SetState(0); }
	for (const TWeakObjectPtr<AWRGraveBossProp>& Tb : St.Tombs) if (Tb.IsValid()) Tb->SetState(0);
	if (AWRGraveBossProp* P = St.Procession.Get()) P->Destroy();
	St.Procession = nullptr;
	St.bPile = St.bFinal = St.bChandelierUsed = St.bSurging = St.bClimbing = St.bDropping = false;
	St.Lime = 0;
	for (int32 k = 0; k < 3; ++k) St.Keystone[k] = GbT(Boss, TEXT("keystoneHealth"), 120.f);
	St.LanternHP = GbT(Boss, TEXT("lanternHealth"), 150.f);
	St.DoorHP = GbT(Boss, TEXT("doorHealth"), 300.f);
	St.BlindUntil = 0.0;
	St.bSpareUsed = false;
	St.bOpeningDue = false;
	St.KneelUntil = 0.0;
	St.NextTollAt = T + 8.0;
	St.NextHatchAt = T + GbT(Boss, TEXT("hatchEvery"), 30.f);
	St.NextRaiseAt = T + GbT(Boss, TEXT("raiseEvery"), 25.f);
	// Its moves back to their phase-1 timing; the stance back from the rope's 60.
	for (FWRBossAttack& A : Boss->Attacks)
	{
		if (const float* W = St.BaseWindup.Find(A.Id)) A.Windup = *W;
		if (const float* H = St.BaseHold.Find(A.Id); H && A.bRow) A.Row.Hold = *H;
	}
	if (St.Kind == TEXT("bell_ringer"))
	{
		if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
		{
			const float Full = Boss->GetDef() ? Boss->GetDef()->Stance : 90.f;
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxStanceAttribute(), Full);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Full);
		}
	}
	if (St.Kind == TEXT("nhal"))
	{
		if (AWRGraveState* GS = AWRGraveState::Get(Boss)) { GS->bChapelDark = false; GS->OnRep_State(); }
	}
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Walking);
	Boss->SetActorHiddenInGame(false);
}

void FWRGraveBoss::EndPlay(AWRBoss* Boss)
{
	if (FWRGraveBossState* St = GbFind(Boss))
	{
		ReleaseGrab(Boss, *St, false);
		for (FWRGraveBuried& B : St->Buried) if (AWRCharacterBase* W = B.Who.Get()) Freed(W);
		for (const TWeakObjectPtr<AWRCreature>& H : St->Hounds) if (H.IsValid()) H->SetHome(H->GetActorLocation());
		if (AWRGraveBossProp* P = St->Procession.Get()) P->Destroy();
	}
	GbStates().Remove(Boss);
}

void FWRGraveBoss::Died(AWRBoss* Boss)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	ReleaseGrab(Boss, St, false);
	for (FWRGraveBuried& B : St.Buried) if (AWRCharacterBase* W = B.Who.Get()) Freed(W);
	St.Buried.Reset();
	// What it raised falls with it (the grave-light that knit them goes out).
	for (const TWeakObjectPtr<AWRCreature>& A : St.Adds)
	{
		if (AWRCreature* C = A.Get(); C && C->IsAlive()) GbHitWith(Boss, C, 100000.f, 0.f, 1, true, false, C->GetActorLocation());
	}
	St.Adds.Reset();
	if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss))
	{
		Body->Caged = nullptr;
		if (St.Kind == TEXT("ossuary_lord")) Body->Pose = 1;   // it lies as its pile, for good
	}
	if (St.Kind == TEXT("nhal"))
	{
		// The chapel crypt's stair opens on her first fall; the lanterns relight; the shades go.
		if (UWRGraveWorld* G = UWRGraveWorld::Get(Boss)) G->OpenChapelCrypt();
		if (AWRGraveState* GS = AWRGraveState::Get(Boss)) { GS->bChapelDark = false; GS->OnRep_State(); }
		if (AWRGraveBossProp* P = St.Procession.Get()) P->Destroy();
		St.Procession = nullptr;
		WRFx::Broadcast(Boss, TEXT("grave.moth_dust"), Boss->GetActorLocation() + FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator, 4.f, TEXT("grave.moth_roar"));
	}
	if (St.Kind == TEXT("gravedigger"))
	{
		for (const TWeakObjectPtr<AWRCreature>& H : St.Hounds) if (H.IsValid()) H->SetHome(H->GetActorLocation());
		St.Hounds.Reset();
	}
	UE_LOG(LogWildreach, Log, TEXT("GRAVE boss %s falls"), *Boss->GetDisplayLabel());
}

// ------------------------------------------------------------------------------------------------ holds

AWRBoss* FWRGraveBoss::HolderOf(const AActor* Victim)
{
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		if (P.Value.Hold.Who.Get() == Victim) return const_cast<AWRBoss*>(P.Key.Get());
	}
	return nullptr;
}

void FWRGraveBoss::Grab(AWRBoss* Boss, FWRGraveBossState& St, AWRCharacterBase* Victim, FName How, float Seconds, float Dps)
{
	if (!Victim || !Victim->IsAlive() || !Boss->IsAlive()) return;
	ReleaseGrab(Boss, St, false);
	const double T = GbNow(Boss);
	St.Hold.Who = Victim;
	St.Hold.How = How;
	St.Hold.Start = T;
	St.Hold.Ends = T + Seconds;
	St.Hold.TickAt = T + 0.5;
	St.Hold.Dps = Dps;
	St.Hold.Free = 0.f;
	St.Hold.Rings = 0;
	Victim->SetActorEnableCollision(false);
	Victim->GetCharacterMovement()->DisableMovement();
	Victim->AttachToActor(Boss, FAttachmentTransformRules::KeepWorldTransform);
	const float R = Boss->GetSimpleCollisionRadius(), H = Boss->GetSimpleCollisionHalfHeight();
	// Where they're held: lifted to his face for the Knell, in the palps, shut in the ribcage, in the cage on his back,
	// wrapped in the veil at her breast.
	FVector Rel(R * 1.1f, 0.f, H * 0.3f);
	if (How == TEXT("knell")) Rel = FVector(R * 0.9f, R * 0.3f, H * 0.7f);
	else if (How == TEXT("palp")) Rel = FVector(R * 0.85f, 0.f, H * 0.2f);
	else if (How == TEXT("ribs")) Rel = FVector(R * 0.15f, 0.f, H * 0.25f);
	else if (How == TEXT("cage")) Rel = FVector(-R * 1.15f, 0.f, H * 0.25f);
	else if (How == TEXT("shroud")) Rel = FVector(R * 0.7f, 0.f, H * 0.35f);
	Victim->SetActorRelativeLocation(Rel);
	if (Victim->Status) Victim->Status->Stagger(Seconds + 0.4f);
	if (How == TEXT("cage"))
	{
		if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) Body->Caged = Victim;
		St.DoorHP = GbT(Boss, TEXT("doorHealth"), 300.f);
	}
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike) Boss->StrikeEnds = FMath::Max(Boss->StrikeEnds, St.Hold.Ends);
	WRFx::Broadcast(Boss, TEXT("grave.grab"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, How == TEXT("cage") ? FName(TEXT("grave.cage_slam")) : FName(TEXT("grave.grab")));
	GbTell(Victim, How == TEXT("cage") ? TEXT("Shut in his cage: break the door, or your friends free you") : TEXT("Held: mash to break free"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE boss %s %s holds %s (%.1f s)"), *Boss->GetDisplayLabel(), *How.ToString(), *Victim->GetName(), Seconds);
}

void FWRGraveBoss::ReleaseGrab(AWRBoss* Boss, FWRGraveBossState& St, bool bThrow)
{
	AWRCharacterBase* V = St.Hold.Who.Get();
	const FName How = St.Hold.How;
	St.Hold = FWRGraveBossHold();
	if (UWRGraveBossBody* Body = Boss ? UWRGraveBossBody::Get(Boss) : nullptr)
	{
		if (Body->Caged == V) Body->Caged = nullptr;
		if ((How == TEXT("ribs") && Body->Pose == 3) || (How == TEXT("shroud") && Body->Pose == 1) || (How == TEXT("cage") && Body->Pose == 3)) Body->Pose = 0;
	}
	if (!V) return;
	V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	V->SetActorEnableCollision(true);
	V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive() && Boss)
	{
		// Dropped in front of it (the Knell's last ring leaves you stunned a moment; the cage's door swings you out).
		V->SetActorLocation(Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 120.f), false, nullptr, ETeleportType::TeleportPhysics);
		V->LaunchCharacter(Boss->GetActorForwardVector() * 400.f + FVector(0.f, 0.f, 200.f), true, true);
		if (How == TEXT("knell") && V->Status) V->Status->Stagger(0.5f);
	}
}

void FWRGraveBoss::TickHold(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	AWRCharacterBase* V = St.Hold.Who.Get();
	if (!V)
	{
		if (!St.Hold.How.IsNone()) St.Hold = FWRGraveBossHold();
		return;
	}
	if (!V->IsAlive() || !Boss->IsAlive())
	{
		ReleaseGrab(Boss, St, false);
		return;
	}
	const double T = GbNow(Boss);
	const FName How = St.Hold.How;
	// Freed by allies: a palp hit for 200, three ribs broken (180), the cage door broken or his back hit for 200.
	const float Need = How == TEXT("palp") ? GbT(Boss, TEXT("snatchFree"), 200.f) : How == TEXT("ribs") ? GbT(Boss, TEXT("ribHealth"), 180.f)
		: How == TEXT("cage") ? GbT(Boss, TEXT("backFree"), 200.f) : 1e9f;
	if (St.Hold.Free >= Need || (How == TEXT("cage") && St.DoorHP <= 0.f))
	{
		WRFx::Broadcast(Boss, TEXT("grave.bone_break"), V->GetActorLocation(), FRotator::ZeroRotator, 1.2f, TEXT("grave.bone_collapse"));
		GbTell(V, TEXT("You're free"));
		ReleaseGrab(Boss, St, false);
		return;
	}
	if (T >= St.Hold.TickAt)
	{
		if (How == TEXT("knell"))
		{
			// He rings the hand-bell against your head three times: each ring a third of the blow and its decay.
			St.Hold.TickAt = T + GbT(Boss, TEXT("knellSeconds"), 1.5f) / GbT(Boss, TEXT("knellRings"), 3.f);
			const float N = GbT(Boss, TEXT("knellRings"), 3.f);
			GbSqueeze(Boss, V, 20.f / N, TEXT("decay"), 40.f / N);
			WRFx::Broadcast(Boss, TEXT("grave.bell_ring"), V->GetActorLocation() + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 1.f, TEXT("grave.handbell"));
			++St.Hold.Rings;
		}
		else
		{
			St.Hold.TickAt = T + 0.5;
			GbSqueeze(Boss, V, St.Hold.Dps * 0.5f, NAME_None, 0.f, How == TEXT("shroud") ? GbT(Boss, TEXT("shroudStamina"), 10.f) * 0.5f : -1.f);
		}
	}
	if (T >= St.Hold.Ends) ReleaseGrab(Boss, St, true);
	(void)Dt;
}

bool FWRGraveBoss::Mash(AWRCharacterBase* Victim)
{
	bool bHeld = false;
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		AWRBoss* Boss = const_cast<AWRBoss*>(P.Key.Get());
		if (!Boss) continue;
		FWRGraveBossState& St = P.Value;
		// Every press fights the hold (the cage only opens from outside, or by its door from in: no mash there).
		if (St.Hold.Who.Get() == Victim)
		{
			bHeld = true;
			if (St.Hold.How != TEXT("cage")) St.Hold.Ends -= 0.15;
		}
		for (FWRGraveBuried& B : St.Buried)
		{
			if (B.Who.Get() == Victim && B.How == TEXT("buried"))
			{
				B.Until -= GbT(Boss, TEXT("buriedMash"), 0.2f);
				bHeld = true;
			}
		}
	}
	return bHeld;
}

bool FWRGraveBoss::IsHeld(const AActor* Who)
{
	if (!Who) return false;
	if (const AWRBoss* B = Cast<AWRBoss>(Who->GetAttachParentActor()); B && IsGraveBoss(B)) return true;
	// Buried: one of the Gravedigger's heaped mounds at your feet.
	for (const TWeakObjectPtr<AWRGraveBossProp>& W : Props())
	{
		const AWRGraveBossProp* P = W.Get();
		if (P && P->Kind == TEXT("dug") && P->State == 1 && FVector::Dist2D(P->GetActorLocation(), Who->GetActorLocation()) < 60.f
			&& FMath::Abs(P->GetActorLocation().Z - GbFeet(Who).Z) < 120.f) return true;
	}
	return false;
}

void FWRGraveBoss::NatureLanded(UWorld* World, const FVector& At, float Radius, AActor* By)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	// Roots up through an open tomb's cracks bind it shut (within the seal reach of the roots' edge).
	const float Reach = WRTune(TEXT("bosses.nhal.sealReach"), 200.f) + Radius;
	for (const TWeakObjectPtr<AWRGraveBossProp>& W : Props())
	{
		AWRGraveBossProp* P = W.Get();
		if (!P || P->GetWorld() != World || P->Kind != TEXT("tomb") || P->State != 1) continue;
		if (FVector::Dist2D(P->GetActorLocation(), At) <= Reach && FMath::Abs(P->GetActorLocation().Z - At.Z) < 400.f) SealTomb(P, By);
	}
	// Roots on the Ossuary Lord's pile hold its bones down for good.
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		AWRBoss* Boss = const_cast<AWRBoss*>(P.Key.Get());
		FWRGraveBossState& St = P.Value;
		if (!Boss || Boss->GetWorld() != World || !St.bPile) continue;
		if (FVector::Dist2D(At, St.PileAt) > Radius + 250.f) continue;
		if (By) St.LastBy = By;
		FinishLord(Boss, St, TEXT("roots bind the bones"));
	}
}

void FWRGraveBoss::ChandelierLanded(AWRGraveBossProp* Chandelier, const FVector& Floor)
{
	if (!Chandelier) return;
	// The chandelier comes down once: under it the Ossuary Lord takes 400 and its stance breaks for 4 s; survivors
	// caught under it are knocked flat (the chain shrieking over the pulley for its whole fall is the tell).
	const UWorld* World = Chandelier->GetWorld();
	const float Reach = WRTune(TEXT("bosses.ossuary_lord.chandelierReach"), 400.f);
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		AWRBoss* Boss = const_cast<AWRBoss*>(P.Key.Get());
		FWRGraveBossState& St = P.Value;
		if (!Boss || Boss->GetWorld() != World || St.Kind != TEXT("ossuary_lord") || St.Chandelier.Get() != Chandelier) continue;
		St.bChandelierUsed = true;
		if (Boss->IsAlive() && !St.bPile && FVector::Dist2D(Boss->GetActorLocation(), Floor) <= Reach + Boss->GetSimpleCollisionRadius())
		{
			FWRHit Hit;
			Hit.Instigator = St.LastBy.Get();
			Hit.Damage = GbT(Boss, TEXT("chandelierDamage"), 400.f);
			Hit.bUnblockable = true;
			Hit.bHeavy = true;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.Origin = Floor + FVector(0.f, 0.f, 600.f);
			Hit.Element = WRTags::Element_Physical;
			Hit.PhysType = 2;
			UWRCombatLibrary::ApplyHit(Boss, Hit);
			UWRCombatLibrary::BreakStance(Boss, GbT(Boss, TEXT("chandelierBreak"), 4.f), St.LastBy.Get());
			UE_LOG(LogWildreach, Log, TEXT("GRAVE the chandelier falls on the Ossuary Lord"));
		}
		for (AWRCharacterBase* C : GbFoes(Boss, Reach, Floor))
		{
			GbHitWith(Boss, C, 40.f, 80.f, 4, true, false, Floor + FVector(0.f, 0.f, 600.f));
		}
	}
}

AWRCharacterBase* FWRGraveBoss::HelpableNear(const UWorld* World, const FVector& At, float Range, FName& OutHow)
{
	AWRCharacterBase* Best = nullptr;
	float BestD = Range;
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		for (const FWRGraveBuried& B : P.Value.Buried)
		{
			AWRCharacterBase* W = B.Who.Get();
			if (!W || W->GetWorld() != World) continue;
			const float D = float(FVector::Dist(W->GetActorLocation(), At));
			if (D < BestD) { BestD = D; Best = W; OutHow = B.How; }
		}
	}
	return Best;
}

void FWRGraveBoss::Freed(AWRCharacterBase* Who)
{
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		for (int32 i = P.Value.Buried.Num() - 1; i >= 0; --i)
		{
			FWRGraveBuried& B = P.Value.Buried[i];
			if (B.Who.Get() != Who) continue;
			if (AWRGraveBossProp* M = B.Mound.Get()) M->Destroy();
			P.Value.Buried.RemoveAt(i);
		}
	}
	if (!Who) return;
	if (Who->Status) Who->Status->ClearStatus(WRTags::Status_Rooted);
	if (UCharacterMovementComponent* Move = Who->GetCharacterMovement(); Move && Move->MovementMode == MOVE_None) Move->SetMovementMode(MOVE_Walking);
	WRFx::Broadcast(Who, TEXT("grave.soil_heave"), GbFeet(Who), FRotator::ZeroRotator, 1.f, TEXT("grave.dig"));
}

void FWRGraveBoss::TickBuried(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	const double T = GbNow(Boss);
	for (int32 i = St.Buried.Num() - 1; i >= 0; --i)
	{
		FWRGraveBuried& B = St.Buried[i];
		AWRCharacterBase* W = B.Who.Get();
		if (!W || !W->IsAlive() || T >= B.Until)
		{
			AWRCharacterBase* Who = W;
			if (AWRGraveBossProp* M = B.Mound.Get()) M->Destroy();
			St.Buried.RemoveAt(i);
			if (Who) Freed(Who);
			continue;
		}
		// Held fast in the earth: no move; buried survivors take 10 a second and their stamina doesn't come back.
		if (W->Status && !W->Status->HasStatus(WRTags::Status_Rooted)) W->Status->ApplyStatus(WRTags::Status_Rooted, float(B.Until - T));
		if (T >= B.TickAt && B.How == TEXT("buried"))
		{
			B.TickAt = T + 0.5;
			GbSqueeze(Boss, W, GbT(Boss, TEXT("buriedDps"), 10.f) * 0.5f, NAME_None, 0.f, 6.f);
		}
	}
	(void)Dt;
}

// ------------------------------------------------------------------------------------------------ the machine's hooks

bool FWRGraveBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRGraveBossState& St = GbS(Boss);
	if (!St.bBegun) Begin(Boss);
	if (St.Kind != TEXT("gravedigger")) return false;
	bSkipWake = false;
	return TickDigger(Boss, St, DeltaSeconds);
}

bool FWRGraveBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRGraveBossState& St = GbS(Boss);
	if (!St.bBegun) Begin(Boss);
	TickHold(Boss, St, DeltaSeconds);
	TickBuried(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("bell_ringer")) return TickRinger(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("crypt_mother")) return TickMother(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("ossuary_lord")) return TickLord(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("gravedigger")) return TickDigger(Boss, St, DeltaSeconds);
	if (St.Kind == TEXT("nhal")) return TickNhal(Boss, St, DeltaSeconds);
	return false;
}

bool FWRGraveBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsGraveBoss(Boss)) return true;
	const FWRGraveBossState* St = GbStates().Find(Boss);
	const UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const FName Id = Attack.Id;
	const float Dist = Target ? float(FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation())) : 0.f;
	if (Boss->CreatureId == TEXT("bell_ringer"))
	{
		const bool bCut = Body && Body->bBroken;
		if (Id == TEXT("toll")) return false;   // only ever forced (on its clock, at the rope)
		if (Id == TEXT("rope_lash")) return !bCut;
		if (Id == TEXT("handbell_ring")) return bCut;
		if (Id == TEXT("headstone_throw")) return Dist >= GbT(Boss, TEXT("headstoneMin"), 400.f);
	}
	if (Boss->CreatureId == TEXT("crypt_mother"))
	{
		if (Id == TEXT("brood_shed")) return Body && Body->Count > 0;
		if (Id == TEXT("ceiling_drop")) return St && St->CeilingZ > 0.f && Dist >= 400.f;
	}
	if (Boss->CreatureId == TEXT("ossuary_lord"))
	{
		if (St && St->bPile) return false;
		if (Id == TEXT("bone_spray")) return Dist >= GbT(Boss, TEXT("sprayMin"), 400.f);
	}
	if (Boss->CreatureId == TEXT("gravedigger"))
	{
		if (Id == TEXT("lantern_swing")) return !(Body && Body->bBroken);   // no lantern to swing while it's smashed
	}
	if (Boss->CreatureId == TEXT("nhal"))
	{
		if (Id == TEXT("the_opening")) return false;   // only ever forced (phase 2's start)
		if (Id == TEXT("lantern_cast")) return Dist >= GbT(Boss, TEXT("castMin"), 1000.f);
		if (Id == TEXT("lay_to_rest")) return GbFurthestTame(Boss, GbT(Boss, TEXT("arenaRadius"), 4000.f)) != nullptr;
		if (St && St->KneelUntil > GbNow(Boss)) return false;
	}
	return true;
}

void FWRGraveBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const FName Id = Attack.Id;
	const double T = GbNow(Boss);
	// The whip sweep found you: her next move comes with no delay hold.
	if (St.Kind == TEXT("crypt_mother") && T < St.FoundUntil && Attack.bRow && Attack.Row.Hold > 0.f)
	{
		Boss->ResolveAt = T + Attack.Windup;
		St.FoundUntil = 0.0;
	}
	if (Id == TEXT("toll") && Body)
	{
		// He wraps the rope round his forearm and leans back; the bell above starts to swing; a deep hum rises.
		Body->Pose = 1;
		if (AWRGraveBossProp* B = St.Belfry.Get()) B->TollAt = B->ServerNow() + Attack.Windup * 0.5f;
		WRFx::Broadcast(Boss, TEXT("grave.bell_hum"), GbRopeFoot(St) + FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator, 1.5f, TEXT("grave.bell_hum"));
	}
	if (Id == TEXT("handbell_ring") && Body) Body->Pose = 2;
	if (Id == TEXT("ceiling_drop"))
	{
		// She climbs a pillar into the dark: chalk dust sifts over one survivor, clicking above.
		St.bClimbing = true;
		St.ClimbFrom = Boss->GetActorLocation();
		if (Body) Body->Pose = 1;
		if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Flying);
		WRFx::Broadcast(Boss, TEXT("grave.chalk_sift"), Boss->AttackPoint + FVector(0.f, 0.f, St.CeilingZ - Boss->AttackPoint.Z - 30.f), FRotator::ZeroRotator, 1.f, TEXT("creature.cryptstalker.click"), Attack.Windup);
	}
	if (Id == TEXT("brood_shed") && Body) Body->Pose = 2;
	if (Id == TEXT("rib_grab") && Body) Body->Pose = 3;
	if (Id == TEXT("mortsafe") && Body) Body->Pose = 3;
	if (Id == TEXT("shroud") && Body) Body->Pose = 1;
	if ((Id == TEXT("keen") || Id == TEXT("unhinged_keen")) && Body && Body->Veil == 0) Body->Veil = 1;
	if (Id == TEXT("unhinged_keen") && Body) Body->Veil = 2;
	if (Id == TEXT("grave_hands"))
	{
		// A circle under every survivor within 20 m, where they stand now: the floor cracks with green light for the last
		// 0.9 s (step out of it).
		St.Hands.Reset();
		const float Reach = GbT(Boss, TEXT("handsReach"), 2000.f), R = GbT(Boss, TEXT("handsRadius"), 250.f);
		for (AWRCharacterBase* C : GbFoes(Boss, Reach, Boss->GetActorLocation()))
		{
			const FVector At = GbFeet(C);
			St.Hands.Add(At);
			const float Tell = 0.9f;
			const float Delay = FMath::Max(0.f, Attack.Windup + (Attack.bRow ? Attack.Row.Hold : 0.f) - Tell);
			TWeakObjectPtr<AWRBoss> WeakB(Boss);
			FTimerHandle H;
			Boss->GetWorldTimerManager().SetTimer(H, [WeakB, At, R, Tell]()
			{
				if (!WeakB.IsValid()) return;
				AWRGroundMarker::Spawn(WeakB->GetWorld(), At + FVector(0, 0, 3), FRotator::ZeroRotator, FVector2D(R, R), false, FLinearColor(0.25f, 0.9f, 0.55f), Tell + 0.2f);
				WRFx::Broadcast(WeakB.Get(), TEXT("grave.floor_crack"), At, FRotator::ZeroRotator, R / 250.f, TEXT("grave.soil_heave"), Tell);
			}, FMath::Max(0.01f, Delay), false);
		}
	}
	if (Id == TEXT("the_opening"))
	{
		// Every lantern in the chapel gutters out, then relights pale green; she lifts the scythe in both hands.
		WRFx::Broadcast(Boss, TEXT("grave.lanterns_gutter"), St.Home + FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, 6.f, TEXT("grave.lanterns_gutter"), Attack.Windup);
	}
	if (Id == TEXT("lay_to_rest"))
	{
		// A grave-light circle opens under the tame furthest from her.
		if (AWRCreature* Tame = GbFurthestTame(Boss, GbT(Boss, TEXT("arenaRadius"), 4000.f)))
		{
			Boss->AttackPoint = GbFeet(Tame);
			AWRGroundMarker::Spawn(Boss->GetWorld(), Boss->AttackPoint + FVector(0, 0, 3), FRotator::ZeroRotator, FVector2D(Attack.Size, Attack.Size), false, FLinearColor(0.25f, 0.9f, 0.55f), Attack.Windup + 0.3f);
		}
	}
	(void)Target;
}

void FWRGraveBoss::Raise(AWRBoss* Boss, FWRGraveBossState& St, FName Id, const FVector& At)
{
	FVector G;
	GbGround(Boss->GetWorld(), At, G);
	const FRotator Face(0.f, (Boss->GetActorLocation() - G).Rotation().Yaw + 180.f, 0.f);
	WRFx::Broadcast(Boss, TEXT("grave.soil_heave"), G, FRotator::ZeroRotator, 1.2f, TEXT("grave.soil_heave"));
	if (!FWRCreatureRegistry::Get().Find(Id)) return;
	if (AWRCreature* C = AWRCreature::SpawnCreature(Boss->GetWorld(), Id, G + FVector(0.f, 0.f, 100.f), Face))
	{
		St.Adds.Add(C);
	}
}

void FWRGraveBoss::Lob(AWRBoss* Boss, const FVector& From, const FVector& To, float Speed, float Damage, float Splash, FName Kind, FName Buildup, float Amount)
{
	FActorSpawnParameters Params;
	Params.Owner = Boss;
	Params.Instigator = Boss;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Velocity = AWRProjectile::LobVelocity(From, To, FMath::Min(Speed, WRTune(TEXT("creatureAI.maxProjectileSpeed"), 1600.f)), 1.f);
	AWRProjectile* P = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Velocity.Rotation(), Params);
	if (!P) return;
	FWRHit Hit;
	Hit.Instigator = Boss;
	Hit.Damage = Damage * GbDamageScale() * (Boss->Phase >= 3 ? GbT(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
	Hit.PoiseDamage = 60.f;
	Hit.Impact = 3;
	Hit.bHeavy = true;
	Hit.Tier = EWRHitTier::Heavy;
	Hit.Element = WRTags::Element_Physical;
	Hit.bProjectile = true;
	if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
	const bool bStone = Kind == TEXT("headstone");
	P->InitLob(Hit, Velocity, 1.f, Splash, bStone ? FString(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_05A.SM_SimpleRock_05A")) : FString());
	TWeakObjectPtr<AWRBoss> WeakB(Boss);
	const float Yaw = (To - From).Rotation().Yaw;
	P->OnLanded = [WeakB, Kind, Yaw](const FVector& At)
	{
		AWRBoss* B = WeakB.Get();
		if (!B) return;
		FWRGraveBossState* St = GbFind(B);
		if (Kind == TEXT("headstone"))
		{
			// The stone stays where it came down: new cover in the yard.
			FVector G;
			GbGround(B->GetWorld(), At, G);
			const FTransform T(FRotator(0.f, Yaw + 90.f, FMath::FRandRange(-12.f, 12.f)), G);
			if (AWRGraveBossProp* S = B->GetWorld()->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				S->Kind = TEXT("headstone");
				S->Index = FMath::RandRange(0, 3);
				S->SetLifeSpan(600.f);
				S->FinishSpawning(T);
			}
			WRFx::Broadcast(B, TEXT("land.dust"), G, FRotator::ZeroRotator, 1.5f, TEXT("stomp"));
		}
		else if (Kind == TEXT("lantern") && St)
		{
			// A grave-fire pool 4 m for 6 s (decay 20 a second while you stand in it).
			FWRGraveBossState::FPool Pool;
			Pool.At = At;
			Pool.Until = GbNow(B) + GbT(B, TEXT("poolSeconds"), 6.f);
			St->Pools.Add(Pool);
			WRFx::Broadcast(B, TEXT("grave.gravefire"), At, FRotator::ZeroRotator, GbT(B, TEXT("poolRadius"), 400.f) / 200.f, TEXT("grave.gravefire"), GbT(B, TEXT("poolSeconds"), 6.f));
		}
	};
}

bool FWRGraveBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRGraveBossState& St = GbS(Boss);
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const FName Id = Attack.Id;
	const double T = GbNow(Boss);
	const FVector Hand = GbBodyPoint(Boss, 0.9f, 0.4f, 0.7f);
	if (Id == TEXT("toll"))
	{
		// The bell tolls: a ring of force 8 m round the tower's base, and three of the yard's dead climb out of their mounds.
		const FVector Base = GbRopeFoot(St);
		if (AWRGraveBossProp* B = St.Belfry.Get()) B->TollAt = B->ServerNow();
		WRFx::Broadcast(Boss, TEXT("grave.toll_ring"), Base, FRotator::ZeroRotator, GbT(Boss, TEXT("tollRing"), 800.f) / 200.f, TEXT("grave.bell_toll"));
		for (AWRCharacterBase* C : GbFoes(Boss, GbT(Boss, TEXT("tollRing"), 800.f), Base))
		{
			GbHitWith(Boss, C, Attack.Damage, Attack.Poise, 3, true, false, Base);
			if (C->Status) C->Status->Stagger(0.6f);
		}
		const int32 Max = int32(GbT(Boss, TEXT("risenMax"), 9.f));
		const FVector Yard = St.Belfry.IsValid() ? St.Belfry->GetActorLocation() : St.Home;
		TArray<int32> Mounds;
		for (int32 i = 0; i < 16; ++i) if (FVector::Dist2D(Yard + FVector(WRGraveBelfry::Mound(i), 0.f), Boss->GetActorLocation()) < GbT(Boss, TEXT("tollReach"), 1500.f) + 600.f) Mounds.Add(i);
		if (Mounds.Num() < 3) for (int32 i = 0; i < 16; ++i) Mounds.AddUnique(i);
		for (int32 k = Mounds.Num() - 1; k > 0; --k) Mounds.Swap(k, FMath::RandRange(0, k));
		for (int32 n = 0; n < int32(GbT(Boss, TEXT("tollRaise"), 3.f)) && n < Mounds.Num() && GbLive(St.Adds) < Max; ++n)
		{
			Raise(Boss, St, TEXT("risen_mourner"), Yard + FVector(WRGraveBelfry::Mound(Mounds[n]), 0.f));
		}
		St.NextTollAt = T + (GbBloodMoon() ? GbT(Boss, TEXT("tollEveryBloodMoon"), 15.f) : GbT(Boss, TEXT("tollEvery"), 20.f));
		St.bToRope = false;
		return true;
	}
	if (Id == TEXT("headstone_throw"))
	{
		// He wrenches a headstone up and hauls it over his head: 1,500 cm/s on a gravity arc, never homing.
		Lob(Boss, Hand + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.6f), Boss->AttackPoint, Attack.bRow ? Attack.Row.ProjectileSpeed : 1500.f, Attack.Damage, Attack.Size, TEXT("headstone"));
		return true;
	}
	if (Id == TEXT("brood_shed"))
	{
		// The young lift off her back and leap toward the survivor furthest from her.
		const int32 N = FMath::Min<int32>(int32(GbT(Boss, TEXT("shedCount"), 6.f)), Body ? Body->Count : 0);
		AWRCharacterBase* Far = GbFurthest(Boss, St.ArenaR + 2000.f);
		const FVector Toward = Far ? Far->GetActorLocation() : Boss->GetActorLocation() + Boss->GetActorForwardVector() * 1000.f;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector At = Boss->GetActorLocation() + FRotator(0.f, 60.f * i, 0.f).Vector() * Boss->GetSimpleCollisionRadius() * 0.5f + FVector(0.f, 0.f, 80.f);
			if (AWRCreature* Y = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("crypt_young"), At, (Toward - At).Rotation()))
			{
				St.Adds.Add(Y);
				Y->LaunchCharacter((Toward - At).GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 500.f), true, true);
			}
		}
		if (Body) { Body->Count = uint8(FMath::Max(0, int32(Body->Count) - N)); Body->Pose = 0; }
		WRFx::Broadcast(Boss, TEXT("grave.brood_shed"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("creature.crypt_mother.shed"));
		return true;
	}
	if (Id == TEXT("ceiling_drop"))
	{
		// She lets go: straight down onto where the dust fell.
		St.bClimbing = false;
		St.bDropping = true;
		return true;
	}
	if (Id == TEXT("bone_spray"))
	{
		// A fan of five shards, 30 deg, 1,500 cm/s, never homing; nothing inside 4 m.
		const int32 N = int32(GbT(Boss, TEXT("sprayShards"), 5.f));
		const float Fan = GbT(Boss, TEXT("sprayFan"), 30.f);
		const FVector From = GbBodyPoint(Boss, 0.6f, 0.f, 0.6f);
		const FVector To = Boss->AttackPoint;
		const float Dist = FMath::Max(GbT(Boss, TEXT("sprayMin"), 400.f), float(FVector::Dist2D(From, To)));
		for (int32 i = 0; i < N; ++i)
		{
			const float A = N > 1 ? -Fan * 0.5f + Fan * i / (N - 1) : 0.f;
			const FVector Dir = FRotator(0.f, (To - From).Rotation().Yaw + A, 0.f).Vector();
			Lob(Boss, From, FVector(From.X, From.Y, To.Z) + Dir * Dist, Attack.bRow ? Attack.Row.ProjectileSpeed : 1500.f, Attack.Damage, 60.f, TEXT("shard"));
		}
		WRFx::Broadcast(Boss, TEXT("grave.bone_burst"), From, FRotator::ZeroRotator, 1.5f, TEXT("grave.bone_rattle"));
		return true;
	}
	if (Id == TEXT("collapse_surge"))
	{
		// Every bone rattles loose at once: it falls into a wave of bone rolling 10 m across the floor (low: jump it) and
		// stands up at the end, behind you.
		St.bSurging = true;
		St.SurgeHit.Reset();
		St.SurgeFrom = Boss->GetActorLocation();
		const FVector Dir = (Boss->AttackPoint - St.SurgeFrom).GetSafeNormal2D();
		St.SurgeTo = St.SurgeFrom + Dir * (FVector::Dist2D(Boss->AttackPoint, St.SurgeFrom) + GbT(Boss, TEXT("surgeBehind"), 300.f));
		const float Len = float(FVector::Dist2D(St.SurgeTo, St.SurgeFrom));
		Boss->StrikeEnds = Boss->StrikeStarted + Len / GbT(Boss, TEXT("surgeSpeed"), 1700.f);
		if (Body) Body->Pose = 2;
		Boss->SetActorEnableCollision(false);
		if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Flying);
		WRFx::Broadcast(Boss, TEXT("grave.bone_wave"), St.SurgeFrom, Dir.Rotation(), 2.f, TEXT("grave.bone_rattle"), float(Boss->StrikeEnds - T));
		return true;
	}
	if (Id == TEXT("bury"))
	{
		// He drives the spade in and heaves up a huge clod: where it lands (locked at the end of the wind-up), a 3 m circle
		// buries everyone in it up to 5 s.
		const FVector At = Boss->AttackPoint;
		WRFx::Broadcast(Boss, TEXT("grave.soil_heave"), At, FRotator::ZeroRotator, Attack.Size / 150.f, TEXT("grave.clod"));
		for (AWRCharacterBase* C : GbFoes(Boss, Attack.Size, At))
		{
			if (FMath::Abs(GbFeet(C).Z - At.Z) > 300.f) continue;
			GbHitWith(Boss, C, Attack.Damage, Attack.Poise, 3, true, false, At + FVector(0.f, 0.f, 300.f));
			FWRGraveBuried B;
			B.Who = C;
			B.How = TEXT("buried");
			B.Until = T + GbT(Boss, TEXT("buriedSeconds"), 5.f);
			B.TickAt = T + 0.5;
			const FTransform MT(FRotator::ZeroRotator, GbFeet(C));
			if (AWRGraveBossProp* M = Boss->GetWorld()->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), MT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				M->Kind = TEXT("dug");
				M->State = 1;   // heaped over someone
				M->SetLifeSpan(GbT(Boss, TEXT("buriedSeconds"), 5.f) + 1.f);
				M->FinishSpawning(MT);
				B.Mound = M;
			}
			St.Buried.Add(B);
			if (C->Status) C->Status->ApplyStatus(WRTags::Status_Rooted, GbT(Boss, TEXT("buriedSeconds"), 5.f));
			GbTell(C, TEXT("Buried! Mash to dig out (a friend holding E digs you out faster)"));
		}
		return true;
	}
	if (Id == TEXT("spade_slam"))
	{
		// The spade comes down and the ground cracks open in a line 8 m forward.
		const FVector From = GbFeet(Boss);
		const FVector Dir = Boss->GetActorForwardVector().GetSafeNormal2D();
		const FVector To = From + Dir * Attack.Range;
		WRFx::Broadcast(Boss, TEXT("grave.ground_crack"), (From + To) * 0.5f, Dir.Rotation(), Attack.Range / 400.f, TEXT("stomp"));
		for (AWRCharacterBase* C : GbFoes(Boss, Attack.Range + 200.f, From))
		{
			const FVector2D AB(To - From), AP(C->GetActorLocation() - From);
			const float U = FMath::Clamp(FVector2D::DotProduct(AP, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
			if (FVector2D::Distance(FVector2D(From) + AB * U, FVector2D(C->GetActorLocation())) > Attack.Size + C->GetSimpleCollisionRadius()) continue;
			GbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, true, From);
		}
		return true;
	}
	if (Id == TEXT("grave_hands"))
	{
		// Bone hands burst up in every circle: 35, rooted 1.5 s, decay 40.
		const float R = GbT(Boss, TEXT("handsRadius"), 250.f);
		for (const FVector& At : St.Hands)
		{
			WRFx::Broadcast(Boss, TEXT("grave.bone_hands"), At, FRotator::ZeroRotator, R / 250.f, TEXT("grave.bone_hands"));
			for (AWRCharacterBase* C : GbFoes(Boss, R, At))
			{
				if (FMath::Abs(GbFeet(C).Z - At.Z) > 250.f) continue;
				GbHitWith(Boss, C, Attack.Damage, Attack.Poise, 3, false, true, At, TEXT("decay"), 40.f);
				if (C->Status) C->Status->ApplyStatus(WRTags::Status_Rooted, GbT(Boss, TEXT("handsRoot"), 1.5f));
			}
		}
		St.Hands.Reset();
		return true;
	}
	if (Id == TEXT("keen") || Id == TEXT("unhinged_keen"))
	{
		St.bKeenSecond = false;
		WRFx::Broadcast(Boss, TEXT("grave.keen"), GbBodyPoint(Boss, 0.4f, 0.f, 0.85f), Boss->GetActorRotation(), 2.f, Id == TEXT("keen") ? FName(TEXT("creature.nhal.keen")) : FName(TEXT("creature.nhal.unhinged")), Attack.bRow ? Attack.Row.Active : 1.f);
		return true;   // its blows land in TickStrike (line of sight)
	}
	if (Id == TEXT("lantern_cast"))
	{
		Lob(Boss, GbBodyPoint(Boss, 0.3f, -0.4f, 1.0f), Boss->AttackPoint, Attack.bRow ? Attack.Row.ProjectileSpeed : 1400.f, Attack.Damage, 200.f, TEXT("lantern"));
		return true;
	}
	if (Id == TEXT("lay_to_rest"))
	{
		// Bone hands drag the tame half into the floor: 40% of its health, held 6 s; a survivor holding E on it for 2 s
		// pulls it out.
		for (AWRCharacterBase* C : GbFoes(Boss, Attack.Size + 100.f, Boss->AttackPoint))
		{
			AWRCreature* Tame = Cast<AWRCreature>(C);
			if (!Tame || !Tame->IsTamed()) continue;
			const UWRAttributeSet* A = Tame->GetAttributes();
			GbSqueeze(Boss, Tame, (A ? A->GetMaxHealth() : 300.f) * GbT(Boss, TEXT("layShare"), 0.4f) / GbDamageScale());
			FWRGraveBuried B;
			B.Who = Tame;
			B.How = TEXT("laid");
			B.Until = T + GbT(Boss, TEXT("layRoot"), 6.f);
			const FTransform MT(FRotator::ZeroRotator, GbFeet(Tame));
			if (AWRGraveBossProp* M = Boss->GetWorld()->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), MT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				M->Kind = TEXT("dug");
				M->State = 2;   // bone hands holding it under
				M->SetLifeSpan(GbT(Boss, TEXT("layRoot"), 6.f) + 1.f);
				M->FinishSpawning(MT);
				B.Mound = M;
			}
			St.Buried.Add(B);
			if (Tame->Status) Tame->Status->ApplyStatus(WRTags::Status_Rooted, GbT(Boss, TEXT("layRoot"), 6.f));
			WRFx::Broadcast(Boss, TEXT("grave.bone_hands"), GbFeet(Tame), FRotator::ZeroRotator, 1.4f, TEXT("grave.bone_hands"));
			GbTell(GbOwnerOf(Tame), TEXT("Your tame is being dragged under: hold E on it to pull it out"));
		}
		return true;
	}
	if (Id == TEXT("the_opening"))
	{
		// She drives the scythe into the floor: the ring (the row's own circle hits), every tomb cracks open, the veil
		// lifts off her in a roaring cloud onto the roof ribs, and the chapel's lanterns die.
		for (const TWeakObjectPtr<AWRGraveBossProp>& Tb : St.Tombs) if (Tb.IsValid() && Tb->State == 0) Tb->SetState(1);
		if (Body) Body->Veil = 1;
		if (AWRGraveState* GS = AWRGraveState::Get(Boss)) { GS->bChapelDark = true; GS->OnRep_State(); }
		WRFx::Broadcast(Boss, TEXT("grave.moth_dust"), Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 5.f, TEXT("grave.moth_roar"));
		St.NextRaiseAt = T + 4.0;
		return false;   // the ring itself is the row's self circle
	}
	return false;
}

bool FWRGraveBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRGraveBossState& St = GbS(Boss);
	const FName Id = Attack.Id;
	const double T = GbNow(Boss);
	if (Id == TEXT("collapse_surge") && St.bSurging)
	{
		// The wave's front carries the bones (the body rides inside it); the front hurts once, low.
		const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
		const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
		const FVector Front = FMath::Lerp(St.SurgeFrom, St.SurgeTo, A);
		Boss->SetActorLocation(Front, false, nullptr, ETeleportType::TeleportPhysics);
		for (AWRCharacterBase* C : GbFoes(Boss, GbT(Boss, TEXT("surgeWidth"), 250.f), Front))
		{
			if (St.SurgeHit.Contains(C)) continue;
			St.SurgeHit.Add(C);
			GbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, true, Front);
		}
		return true;
	}
	if (Id == TEXT("ceiling_drop") && St.bDropping)
	{
		const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
		const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
		FVector G;
		GbGround(Boss->GetWorld(), Boss->AttackPoint, G, 200.f, 2000.f);
		const FVector Floor = G + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
		const FVector Top(Floor.X, Floor.Y, FMath::Max(Floor.Z, St.CeilingZ - Boss->GetSimpleCollisionHalfHeight()));
		Boss->SetActorLocation(FMath::Lerp(Top, Floor, A * A), false, nullptr, ETeleportType::TeleportPhysics);
		if (A >= 1.f)
		{
			St.bDropping = false;
			if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Walking);
			if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) Body->Pose = 0;
			WRFx::Broadcast(Boss, TEXT("land.dust"), G, FRotator::ZeroRotator, 3.f, TEXT("stomp"));
			for (AWRCharacterBase* C : GbFoes(Boss, Attack.Size, G)) GbHitWith(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, false, false, G + FVector(0.f, 0.f, 300.f));
		}
		return true;
	}
	if (Id == TEXT("keen") || Id == TEXT("unhinged_keen"))
	{
		// The Keen: a cone 25 m x 70 deg from her face, unblockable; pillars and tombs block it (line of sight). The
		// Unhinged keen first rings 12 m all round her, then the cone.
		const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
		const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
		const bool bRing = Id == TEXT("unhinged_keen") && A < 0.5f;
		if (!bRing && St.bKeenSecond) return true;
		if (!bRing) St.bKeenSecond = true;
		const FVector Mouth = GbBodyPoint(Boss, 0.4f, 0.f, 0.85f);
		const float Reach = bRing ? Attack.Size : 2500.f;
		const float Half = GbT(Boss, TEXT("keenAngle"), 35.f);
		for (AWRCharacterBase* C : GbFoes(Boss, Reach, Boss->GetActorLocation()))
		{
			if (Boss->StrikeHits.Contains(C) && !bRing) continue;
			const FVector To = C->GetActorLocation() - Mouth;
			if (!bRing && FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(To.GetSafeNormal2D(), Boss->GetActorForwardVector().GetSafeNormal2D()))) > Half) continue;
			FHitResult Block;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGraveKeen), false, Boss);
			Q.AddIgnoredActor(C);
			if (Boss->GetWorld()->LineTraceSingleByChannel(Block, Mouth, C->GetActorLocation(), ECC_WorldStatic, Q) && !Cast<APawn>(Block.GetActor())) continue;   // behind a pillar or a tomb
			if (bRing && Boss->StrikeHits.Contains(C)) continue;
			Boss->StrikeHits.Add(C);
			GbHitWith(Boss, C, Attack.Damage, Attack.Poise, 2, true, false, Mouth, TEXT("decay"), Attack.Buildup > 0.f ? Attack.Buildup : (Id == TEXT("keen") ? 80.f : 60.f), Id == TEXT("keen") ? 40.f : -1.f);
		}
		if (bRing && A >= 0.45f) Boss->StrikeHits.Reset();   // the cone may hit those the ring did
		return true;
	}
	if (Id == TEXT("toll") || Id == TEXT("headstone_throw") || Id == TEXT("brood_shed") || Id == TEXT("bone_spray") || Id == TEXT("bury") || Id == TEXT("spade_slam")
		|| Id == TEXT("grave_hands") || Id == TEXT("lantern_cast") || Id == TEXT("lay_to_rest") || Id == TEXT("ceiling_drop"))
	{
		return true;   // done at the strike (BeginStrike)
	}
	(void)DeltaSeconds;
	return false;
}

void FWRGraveBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const FName Id = Attack.Id;
	if (Id == TEXT("collapse_surge") && St.bSurging)
	{
		St.bSurging = false;
		Boss->SetActorLocation(St.SurgeTo + FVector(0.f, 0.f, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
		Boss->SetActorEnableCollision(true);
		if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->SetMovementMode(MOVE_Walking);
		// It stands up facing back the way it came (behind you).
		Boss->SetActorRotation(FRotator(0.f, (St.SurgeFrom - St.SurgeTo).Rotation().Yaw, 0.f));
		if (Body) Body->Pose = 0;
	}
	if (Body && (Id == TEXT("toll") || Id == TEXT("handbell_ring") || Id == TEXT("brood_shed"))) Body->Pose = 0;
	if (Body && (Id == TEXT("keen")) && Boss->Phase < 2) Body->Veil = 0;
	if (Body && Id == TEXT("unhinged_keen")) Body->Veil = 1;
	if (Body && (Id == TEXT("rib_grab") || Id == TEXT("mortsafe") || Id == TEXT("shroud")) && St.Hold.Who == nullptr) Body->Pose = 0;
}

void FWRGraveBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (!IsGraveBoss(Boss)) return;
	// Their blows are grave-light's: decay where the row says it (attacks.json buildup), the Keen's stamina.
	if (Attack.bRow && Attack.Row.StaminaDamage > 0.f) Hit.StaminaDamage = Attack.Row.StaminaDamage;
}

void FWRGraveBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsGraveBoss(Boss) || !Victim || !Victim->IsAlive() || Result != EWRHitResult::Hit) return;
	FWRGraveBossState& St = GbS(Boss);
	if (AttackId == TEXT("knell")) Grab(Boss, St, Victim, TEXT("knell"), GbT(Boss, TEXT("knellSeconds"), 1.5f), 0.f);
	else if (AttackId == TEXT("palp_snatch")) Grab(Boss, St, Victim, TEXT("palp"), GbT(Boss, TEXT("snatchSeconds"), 2.f), GbT(Boss, TEXT("snatchDps"), 18.f));
	else if (AttackId == TEXT("rib_grab")) Grab(Boss, St, Victim, TEXT("ribs"), GbT(Boss, TEXT("ribHold"), 3.f), GbT(Boss, TEXT("ribDps"), 12.f));
	else if (AttackId == TEXT("mortsafe")) Grab(Boss, St, Victim, TEXT("cage"), GbT(Boss, TEXT("cageSeconds"), 6.f), GbT(Boss, TEXT("cageDps"), 8.f));
	else if (AttackId == TEXT("shroud")) Grab(Boss, St, Victim, TEXT("shroud"), GbT(Boss, TEXT("shroudSeconds"), 3.f), GbT(Boss, TEXT("shroudDps"), 15.f));
	else if (AttackId == TEXT("whip_sweep")) St.FoundUntil = GbNow(Boss) + GbT(Boss, TEXT("foundSeconds"), 8.f);
}

float FWRGraveBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return Damage;
	FWRGraveBossState& St = GbS(Boss);
	AActor* By = Hit.Instigator.Get();
	if (By) St.LastBy = By;
	const FName K = St.Kind;
	const FVector At = GbHitFrom(Boss, Hit);
	// Allies' blows on the body that holds a friend count toward freeing them (the palp, the ribs, his back).
	if (St.Hold.Who.IsValid() && By && By != St.Hold.Who.Get() && GbSurvivorSide(By))
	{
		if (St.Hold.How == TEXT("palp") || St.Hold.How == TEXT("ribs")) St.Hold.Free += Damage;
		if (St.Hold.How == TEXT("cage"))
		{
			// The door (on his back) or his back itself.
			if (FVector::Dist(At, GbCageAt(Boss)) < 250.f) St.DoorHP -= Damage;
			else if (FVector::DotProduct((At - Boss->GetActorLocation()).GetSafeNormal2D(), Boss->GetActorForwardVector()) < -0.3f) St.Hold.Free += Damage;
		}
	}
	// From inside: the caged hit the door at half; the one in the ribcage hits the chest keystone at x2.
	if (St.Hold.Who.IsValid() && By == St.Hold.Who.Get())
	{
		if (St.Hold.How == TEXT("cage")) { St.DoorHP -= Damage * GbT(Boss, TEXT("insideDoor"), 0.5f); return 0.f; }
		if (St.Hold.How == TEXT("ribs")) Damage *= GbT(Boss, TEXT("chestInside"), 2.f);
	}
	if (K == TEXT("crypt_mother") && GbIsFire(Hit))
	{
		// Fire on her back kills the young riding there (10 a fire blow): a Fire player ends Brood shed early.
		if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss); Body && Body->Count > 0)
		{
			Body->Count = uint8(FMath::Max(0, int32(Body->Count) - int32(GbT(Boss, TEXT("fireKillsRiders"), 10.f))));
			WRFx::Broadcast(Boss, TEXT("grave.young_burn"), GbBodyPoint(Boss, 0.f, 0.f, 0.9f), FRotator::ZeroRotator, 2.f, TEXT("creature.crypt_young.death"));
		}
	}
	if (K == TEXT("gravedigger") && St.LanternHP > 0.f && FVector::Dist(At, GbLanternAt(Boss)) < 160.f + Boss->GetSimpleCollisionRadius() * 0.2f)
	{
		// His lantern (150): smashed, he hunts the last place he saw you for 10 s, then lights a spare (once a fight).
		St.LanternHP -= Damage;
		if (St.LanternHP <= 0.f)
		{
			St.LanternHP = 0.f;
			St.BlindUntil = GbNow(Boss) + GbT(Boss, TEXT("blindSeconds"), 10.f);
			St.LastSeen = By ? By->GetActorLocation() : Boss->GetActorLocation();
			if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) Body->bBroken = true;
			WRFx::Broadcast(Boss, TEXT("grave.lantern_smash"), GbLanternAt(Boss), FRotator::ZeroRotator, 1.5f, TEXT("grave.glass_break"));
			CancelAttack(Boss);
			UE_LOG(LogWildreach, Log, TEXT("GRAVE the Gravedigger's lantern is smashed"));
		}
	}
	if (K == TEXT("ossuary_lord"))
	{
		if (St.bPile)
		{
			// The pile: blows go to its three glowing keystones (120 each, strike x1.5); a Nature root on the pile ends it.
			if (GbIsNature(Hit) || Hit.Buildups.FindRef(TEXT("root")) > 0.f)
			{
				FinishLord(Boss, St, TEXT("roots bind the bones"));
				return 0.f;
			}
			int32 Best = -1;
			float BestD = 1e9f;
			for (int32 i = 0; i < 3; ++i)
			{
				if (St.Keystone[i] <= 0.f) continue;
				const float D = float(FVector::Dist(At, GbKeystoneAt(St, i)));
				if (D < BestD) { BestD = D; Best = i; }
			}
			if (Best >= 0 && BestD < 400.f)
			{
				St.Keystone[Best] -= Damage * (Hit.PhysType == 2 ? GbT(Boss, TEXT("keystoneStrike"), 1.5f) : 1.f);
				if (St.Keystone[Best] <= 0.f)
				{
					if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) Body->Count |= uint8(1 << Best);
					WRFx::Broadcast(Boss, TEXT("grave.keystone_smash"), GbKeystoneAt(St, Best), FRotator::ZeroRotator, 1.f, TEXT("grave.bone_collapse"));
				}
				if (St.Keystone[0] <= 0.f && St.Keystone[1] <= 0.f && St.Keystone[2] <= 0.f) FinishLord(Boss, St, TEXT("its keystones are smashed"));
			}
			return 0.f;
		}
		const UWRAttributeSet* A = Boss->GetAttributes();
		if (A && !St.bFinal && Damage >= A->GetHealth())
		{
			// At 0 it collapses into a bone pile (and re-forms in 8 s at half health unless stopped).
			Collapse(Boss, St);
			return FMath::Max(0.f, A->GetHealth() - 1.f);
		}
	}
	if (K == TEXT("nhal"))
	{
		// Weak to Nature (x1.5); decay builds on her at half; in phase 2 hits on the open jaw count as head crits (the
		// row's lock point does the crit).
		if (GbIsNature(Hit))
		{
			Damage *= GbT(Boss, TEXT("natureMult"), 1.5f);
			// A Nature blow landing within 2 m of an open tomb seals it too.
			NatureLanded(Boss->GetWorld(), At, 0.f, By);
		}
	}
	return Damage;
}

bool FWRGraveBoss::OwnWeakness(const AWRBoss* Boss)
{
	// None of them takes the island's fire/frost rule: Nhal's weakness is Nature, the minis' counters are their arenas.
	return IsGraveBoss(Boss);
}

void FWRGraveBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	(void)Boss; (void)Hit; (void)Damage;
}

void FWRGraveBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsGraveBoss(Boss) || !Boss->HasAuthority()) return;
	FWRGraveBossState& St = GbS(Boss);
	if (St.Kind != TEXT("nhal")) return;
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	if (NewPhase == 2)
	{
		// Every phase-1 wind-up 10% quicker; the Reaping arc and Grave hands gain their delay holds (up to 0.6 s).
		for (FWRBossAttack& A : Boss->Attacks)
		{
			if (A.MinPhase <= 1) A.Windup *= GbT(Boss, TEXT("p2Windup"), 0.9f);
			if ((A.Id == TEXT("reaping_arc") || A.Id == TEXT("grave_hands")) && A.bRow) A.Row.Hold = GbT(Boss, TEXT("p2Hold"), 0.6f);
		}
		St.bOpeningDue = true;
	}
	if (NewPhase == 3) StartProcession(Boss, St);
}

void FWRGraveBoss::StartProcession(AWRBoss* Boss, FWRGraveBossState& St)
{
	// She falls to her knees; her body empties into shade: forty Lantern Mourners pour out of her and walk a ring round
	// the nave; the broken tower tolls every 10 s and turns them back. Nhal's shade rises and fights inside the ring.
	CancelAttack(Boss);
	St.KneelUntil = GbNow(Boss) + 3.0;
	if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) { Body->Pose = 2; Body->Veil = 3; }
	const FTransform T(FRotator(0.f, St.Yaw, 0.f), St.Home);
	if (AWRGraveBossProp* P = Boss->GetWorld()->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		P->Kind = TEXT("procession");
		P->OwnerBoss = Boss;
		P->StartAt = P->ServerNow();
		P->TurnAt = P->StartAt;
		P->TurnAngle = 0.f;
		P->Dir = 1.f;
		P->FinishSpawning(T);
		St.Procession = P;
	}
	St.NextShadeToll = GbNow(Boss) + GbT(Boss, TEXT("tollEvery"), 10.f);
	WRFx::Broadcast(Boss, TEXT("grave.shade_pour"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("creature.nhal.procession"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE Nhal: the Procession"));
}

bool FWRGraveBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!IsGraveBoss(Boss)) return false;
	if (Boss->CreatureId == TEXT("gravedigger")) return true;   // he roams: no stone (as the island's roamers)
	FVector At = FVector::ZeroVector;
	for (const FWRGraveData::FRunback& R : FWRGraveData::Get().Runback) if (R.Boss == Boss->CreatureId) At = R.At;
	if (At.IsNearlyZero())
	{
		// The catacomb halls: the stone by the hall's door, at the foot of the gallery that comes in.
		FVector C;
		float Rad = 0.f, Yaw = 0.f;
		if (!ArenaOf(Boss->CreatureId, C, Rad, Yaw)) return false;
		At = C + FRotator(0.f, Yaw + 180.f, 0.f).Vector() * (Rad + 500.f);
	}
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
	{
		if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	}
	FVector G;
	if (!GbGround(Boss->GetWorld(), At, G, 400.f, 3000.f)) return false;
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

FName FWRGraveBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == TEXT("nhal")) return TEXT("mourners_lantern");
	(void)bScarred;
	return Fallback;
}

bool FWRGraveBoss::HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire)
{
	AWRGraveBossProp* P = Cast<AWRGraveBossProp>(Thing);
	if (!P || !P->HasAuthority()) return false;
	FWRHit Hit;
	Hit.Instigator = By;
	Hit.Damage = Damage;
	Hit.bMelee = By && FVector::Dist(By->GetActorLocation(), P->HitPoint()) < 800.f;
	if (bFire) Hit.Element = WRTags::Element_Fire;
	(void)Comp;
	return P->TakeHit(By, Damage, &Hit, P->HitPoint());
}

void FWRGraveBoss::LimeLanded(UWorld* World, const FVector& At, AActor* Thrower)
{
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRGraveBossState>& P : GbStates())
	{
		AWRBoss* Boss = const_cast<AWRBoss*>(P.Key.Get());
		FWRGraveBossState& St = P.Value;
		if (!Boss || Boss->GetWorld() != World || !St.bPile || FVector::Dist2D(At, St.PileAt) > GbT(Boss, TEXT("limeReach"), 450.f)) continue;
		// Quicklime stops the grave-light knitting: two on the pile end it.
		++St.Lime;
		WRFx::Broadcast(Boss, TEXT("grave.lime_steam"), St.PileAt, FRotator::ZeroRotator, 2.f, TEXT("grave.lime_hiss"));
		if (Thrower) St.LastBy = Thrower;
		if (St.Lime >= int32(GbT(Boss, TEXT("limePots"), 2.f))) FinishLord(Boss, St, TEXT("quicklime"));
	}
}

// ------------------------------------------------------------------------------------------------ tombs and seals

AWRGraveBossProp* FWRGraveBoss::OpenTombNear(const UWorld* World, const FVector& At, float Range)
{
	AWRGraveBossProp* Best = nullptr;
	float BestD = Range;
	for (const TWeakObjectPtr<AWRGraveBossProp>& W : Props())
	{
		AWRGraveBossProp* P = W.Get();
		if (!P || P->GetWorld() != World || P->Kind != TEXT("tomb") || P->State != 1) continue;
		const float D = float(FVector::Dist2D(P->GetActorLocation(), At));
		if (D < BestD && FMath::Abs(P->GetActorLocation().Z - At.Z) < 400.f) { BestD = D; Best = P; }
	}
	return Best;
}

void FWRGraveBoss::SealTomb(AWRGraveBossProp* Tomb, AActor* By)
{
	if (!Tomb || Tomb->State != 1) return;
	// Roots burst from the cracks and bind the slab shut for the rest of the fight, with white blossom over it.
	Tomb->SetState(2);
	WRFx::Broadcast(Tomb, TEXT("grave.tomb_seal"), Tomb->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("forest.uproot"));
	GbTell(By, TEXT("The tomb is sealed"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE a chapel tomb (%d) is sealed by %s"), Tomb->Index, *GetNameSafe(By));
}

// ================================================================================================ the five fights

bool FWRGraveBoss::Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target)
{
	if (!Boss || !Target || Boss->Current != INDEX_NONE) return false;
	const int32 i = Boss->Attacks.IndexOfByPredicate([Id](const FWRBossAttack& A) { return A.Id == Id; });
	if (!Boss->Attacks.IsValidIndex(i) || Boss->Attacks[i].MinPhase > Boss->Phase) return false;
	Boss->StartAttack(Boss->Attacks[i], Target);
	return true;
}

void FWRGraveBoss::CancelAttack(AWRBoss* Boss)
{
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike) EndStrike(Boss, Boss->Attacks[Boss->Current]);
	Boss->Current = INDEX_NONE;
	Boss->StrikeState = AWRBoss::EStrike::None;
	Boss->SpeedBoost = 1.f;
}

bool FWRGraveBoss::TickRinger(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	// The Toll comes on its clock while the rope holds: he goes back to the rope (a chance to catch him on the way),
	// wraps it round his forearm and leans back.
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const bool bCut = Body && Body->bBroken;
	const double T = GbNow(Boss);
	if (bCut || T < St.NextTollAt || Boss->Current != INDEX_NONE || St.Hold.Who.IsValid()) return false;
	const FVector Rope = GbRopeFoot(St) + FVector(260.f, 0.f, 0.f);
	if (FVector::Dist2D(Boss->GetActorLocation(), Rope) > 450.f)
	{
		St.bToRope = true;
		GbMoveTo(Boss, Rope, 1.f);
		return true;
	}
	AWRCharacterBase* Any = nullptr;
	for (AWRCharacterBase* C : GbFoes(Boss, St.ArenaR + 2000.f, Boss->GetActorLocation())) { Any = C; break; }
	if (Any) Force(Boss, TEXT("toll"), Any);
	(void)Dt;
	return false;
}

bool FWRGraveBoss::TickMother(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	const double T = GbNow(Boss);
	// The gallery's ceiling (for the drop): the room's roof over her.
	if (St.CeilingZ <= 0.f)
	{
		if (const FWRGraveData::FRoom* R = WRGrave::RoomAt(Boss->GetActorLocation())) St.CeilingZ = R->At.Z + R->H;
	}
	// The sacs: a lit torch held to one 2 s burns it out; whole ones hatch two young every 30 s.
	const float Reach = GbT(Boss, TEXT("torchReach"), 220.f);
	for (int32 i = 0; i < St.Sacs.Num(); ++i)
	{
		AWRGraveBossProp* S = St.Sacs[i].Get();
		if (!S || S->State != 0) continue;
		bool bTorch = false;
		for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It && !bTorch; ++It)
		{
			bTorch = It->IsAlive() && FVector::Dist(It->GetActorLocation(), S->HitPoint()) < Reach + 80.f && UWRSurvivalComponent::HoldsTorch(*It);
		}
		if (!bTorch) { St.SacTorch[i] = 0.0; continue; }
		if (St.SacTorch[i] <= 0.0) { St.SacTorch[i] = T; WRFx::Broadcast(S, TEXT("grave.sac_singe"), S->HitPoint(), FRotator::ZeroRotator, 1.f, TEXT("forest.thorn_burn")); }
		if (T - St.SacTorch[i] >= GbT(Boss, TEXT("torchSeconds"), 2.f)) { S->Health = 0.f; S->SetState(1); }
	}
	if (T >= St.NextHatchAt)
	{
		St.NextHatchAt = T + GbT(Boss, TEXT("hatchEvery"), 30.f);
		const int32 Max = int32(GbT(Boss, TEXT("youngMax"), 16.f));
		for (const TWeakObjectPtr<AWRGraveBossProp>& W : St.Sacs)
		{
			AWRGraveBossProp* S = W.Get();
			if (!S || S->State != 0) continue;
			for (int32 n = 0; n < int32(GbT(Boss, TEXT("hatchCount"), 2.f)) && GbLive(St.Adds) < Max; ++n)
			{
				FVector G;
				GbGround(Boss->GetWorld(), S->HitPoint(), G, 100.f, 600.f);
				if (AWRCreature* Y = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("crypt_young"), G + FVector(0.f, 0.f, 40.f) + FRotator(0.f, 120.f * n, 0.f).Vector() * 60.f, FRotator(0.f, 120.f * n, 0.f)))
				{
					St.Adds.Add(Y);
				}
			}
			WRFx::Broadcast(S, TEXT("grave.sac_hatch"), S->HitPoint(), FRotator::ZeroRotator, 1.f, TEXT("creature.crypt_young.hatch"));
		}
	}
	// Climbing to the ceiling for the drop (the wind-up): up the nearest dark, then over the dust.
	if (St.bClimbing && Boss->StrikeState == AWRBoss::EStrike::Windup && St.CeilingZ > 0.f)
	{
		const float Span = FMath::Max(0.1f, float(Boss->ResolveAt - Boss->StrikeStarted));
		const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
		const float Up = FMath::Clamp(A / GbT(Boss, TEXT("dropClimb"), 0.6f), 0.f, 1.f);
		const float Over = FMath::Clamp((A - 0.4f) / 0.6f, 0.f, 1.f);
		const FVector Top(FMath::Lerp(St.ClimbFrom.X, Boss->AttackPoint.X, Over), FMath::Lerp(St.ClimbFrom.Y, Boss->AttackPoint.Y, Over), St.CeilingZ - Boss->GetSimpleCollisionHalfHeight() - 10.f);
		Boss->SetActorLocation(FVector(Top.X, Top.Y, FMath::Lerp(St.ClimbFrom.Z, Top.Z, FMath::SmoothStep(0.f, 1.f, Up))), false, nullptr, ETeleportType::TeleportPhysics);
		return false;
	}
	(void)Dt;
	return false;
}

void FWRGraveBoss::Collapse(AWRBoss* Boss, FWRGraveBossState& St)
{
	CancelAttack(Boss);
	ReleaseGrab(Boss, St, false);
	St.bPile = true;
	St.Lime = 0;
	St.PileUntil = GbNow(Boss) + GbT(Boss, TEXT("reformSeconds"), 8.f);
	St.PileAt = GbFeet(Boss);
	St.Yaw = Boss->GetActorRotation().Yaw;
	for (int32 k = 0; k < 3; ++k) St.Keystone[k] = GbT(Boss, TEXT("keystoneHealth"), 120.f);
	if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) { Body->Pose = 1; Body->Count = 0; }
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->StopMovementImmediately();
	WRFx::Broadcast(Boss, TEXT("grave.bone_collapse"), St.PileAt, FRotator::ZeroRotator, 3.f, TEXT("grave.bone_collapse"));
	for (AWRCharacterBase* C : GbFoes(Boss, 3000.f, St.PileAt)) GbTell(C, TEXT("It falls to bones... and the grave-light is knitting them back"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE the Ossuary Lord collapses into its pile"));
}

void FWRGraveBoss::Reform(AWRBoss* Boss, FWRGraveBossState& St)
{
	St.bPile = false;
	GbSetHealth(Boss, GbT(Boss, TEXT("reformHealth"), 0.5f));
	if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) { Body->Pose = 0; Body->Count = 0; }
	WRFx::Broadcast(Boss, TEXT("grave.bone_reform"), St.PileAt, FRotator::ZeroRotator, 3.f, TEXT("grave.bone_rattle"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE the Ossuary Lord re-forms"));
}

void FWRGraveBoss::FinishLord(AWRBoss* Boss, FWRGraveBossState& St, const TCHAR* Why)
{
	if (St.bFinal) return;
	St.bFinal = true;
	St.bPile = false;
	UE_LOG(LogWildreach, Log, TEXT("GRAVE the Ossuary Lord is ended (%s)"), Why);
	WRFx::Broadcast(Boss, TEXT("grave.bone_collapse"), St.PileAt, FRotator::ZeroRotator, 4.f, TEXT("grave.bone_collapse"));
	FWRHit Kill;
	Kill.Instigator = St.LastBy.Get() ? St.LastBy.Get() : Boss;
	Kill.Damage = 1e7f;
	Kill.bUnblockable = true;
	Kill.bDamageOverTime = true;
	Kill.Origin = St.PileAt;
	Kill.Element = WRTags::Element_Physical;
	UWRCombatLibrary::ApplyHit(Boss, Kill);
}

bool FWRGraveBoss::TickLord(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	const double T = GbNow(Boss);
	if (St.bPile)
	{
		// Eight seconds in its pile: roots, quicklime or the keystones end it; otherwise it stands up again at half.
		if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->StopMovementImmediately();
		if (T >= St.PileUntil) Reform(Boss, St);
		return true;
	}
	// (The chandelier, once: its chain cut while it stands under it lands on it: FWRGraveBoss::ChandelierLanded.)
	(void)Dt;
	return false;
}

bool FWRGraveBoss::TickDigger(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss);
	const double T = GbNow(Boss);
	const FWRGraveData& D = FWRGraveData::Get();
	const bool bFighting = Boss->bEngaged;
	// His lantern out: he swings at the last place he saw you for 10 s, then lights a spare from the cage (once).
	if (bFighting && St.BlindUntil > 0.0)
	{
		if (T < St.BlindUntil)
		{
			if (Boss->Current == INDEX_NONE)
			{
				if (FVector::Dist2D(Boss->GetActorLocation(), St.LastSeen) > 450.f) GbMoveTo(Boss, St.LastSeen, 1.f);
				else
				{
					AWRCharacterBase* Near = nullptr;
					for (AWRCharacterBase* C : GbFoes(Boss, 700.f, Boss->GetActorLocation())) { Near = C; break; }
					if (Near) Force(Boss, TEXT("spade_arc"), Near);
					else if (Boss->Current == INDEX_NONE)
					{
						// Nothing there: he swings at the air where you were.
						if (AWRCharacterBase* Any = GbFurthest(Boss, 6000.f)) { St.LastSeen = St.LastSeen + (Any->GetActorLocation() - St.LastSeen).GetSafeNormal2D() * 200.f; }
					}
				}
			}
			return Boss->Current == INDEX_NONE;
		}
		St.BlindUntil = 0.0;
		if (!St.bSpareUsed)
		{
			St.bSpareUsed = true;
			St.LanternHP = GbT(Boss, TEXT("lanternHealth"), 150.f);
			if (Body) Body->bBroken = false;
			WRFx::Broadcast(Boss, TEXT("grave.lantern_light"), GbLanternAt(Boss), FRotator::ZeroRotator, 1.f, TEXT("grave.toro_light"));
		}
	}
	if (bFighting) return false;
	// Out of a fight: by day he sleeps in the Watch House, sitting against the barred door; at dusk he walks his round
	// (Watch House, the Row of the Named, the Dog Graves, the Great Yew, the Ford, the Lime Kilns, back), stopping to
	// dig at six graves; Gravehounds trail him 30 m back. Blood Moon: two more stops.
	const AWRGameState* GS = AWRGameState::Get(Boss);
	const float Hour = GS ? GS->GetHour() : 12.f;
	const bool bNight = Hour >= GbT(Boss, TEXT("duskHour"), 19.f) || Hour < GbT(Boss, TEXT("dawnHour"), 5.f);
	const TArray<FVector>& Route = D.GravediggerRoute;
	if (bNight && !St.bOnRound && Route.Num() > 1)
	{
		St.bOnRound = true;
		St.Route = 0;
		St.StopIndex = 0;
		St.StopsAt.Reset();
		// The stops: the nearest route point to each of the six graves (and two more on a Blood Moon).
		TArray<FVector> Stops = D.GravediggerStops;
		if (GbBloodMoon()) for (int32 e = 0; e < 2; ++e) Stops.Add(Route[FMath::RandRange(Route.Num() / 5, Route.Num() * 4 / 5)]);
		for (const FVector& S : Stops)
		{
			int32 Best = 0;
			float BestD = 1e12f;
			for (int32 i = 0; i < Route.Num(); ++i) { const float Dd = float(FVector::DistSquared2D(Route[i], S)); if (Dd < BestD) { BestD = Dd; Best = i; } }
			St.StopsAt.AddUnique(Best);
		}
		St.StopsAt.Sort();
		if (Body) Body->Pose = 0;
		// His hounds rise to follow him.
		St.Hounds.RemoveAll([](const TWeakObjectPtr<AWRCreature>& H) { return !H.IsValid() || !H->IsAlive(); });
		for (int32 h = St.Hounds.Num(); h < int32(GbT(Boss, TEXT("hounds"), 3.f)); ++h)
		{
			const FVector At = Boss->GetActorLocation() - Boss->GetActorForwardVector() * 800.f + FRotator(0.f, 120.f * h, 0.f).Vector() * 300.f;
			FVector G;
			GbGround(Boss->GetWorld(), At, G);
			if (AWRCreature* Hd = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("gravehound"), G + FVector(0.f, 0.f, 80.f), Boss->GetActorRotation())) St.Hounds.Add(Hd);
		}
		UE_LOG(LogWildreach, Log, TEXT("GRAVE the Gravedigger starts his round (%d stops)"), St.StopsAt.Num());
	}
	if (!St.bOnRound)
	{
		// Asleep at the Watch House (a sneak attack opens on him).
		if (FVector::Dist2D(Boss->GetActorLocation(), D.GravediggerLair) > 400.f && Route.Num() > 0)
		{
			GbMoveTo(Boss, D.GravediggerLair, 0.6f);
			if (Body) Body->Pose = 0;
		}
		else if (Body) Body->Pose = 1;
		return true;
	}
	// Digging at a stop (3 game minutes): the spade's scrape carries 40 m; the grave is left open.
	if (T < St.DigUntil)
	{
		if (Body) Body->Pose = 2;
		if (FMath::Fmod(T, 1.6) < Dt) WRFx::Broadcast(Boss, TEXT("grave.dig_spoil"), GbBodyPoint(Boss, 1.2f, 0.f, -0.9f), FRotator::ZeroRotator, 1.5f, TEXT("grave.spade"));
		return true;
	}
	if (Body && Body->Pose == 2) Body->Pose = 0;
	if (!Route.IsValidIndex(St.Route))
	{
		St.bOnRound = false;
		return true;
	}
	if (St.StopsAt.IsValidIndex(St.StopIndex) && St.Route >= St.StopsAt[St.StopIndex])
	{
		// A stop: he digs.
		St.DigUntil = T + GbT(Boss, TEXT("digMinutes"), 3.f) * 2.5;   // a game minute is 2.5 s
		++St.StopIndex;
		const FVector Grave = GbBodyPoint(Boss, 1.8f, 0.f, -1.f);
		FVector G;
		GbGround(Boss->GetWorld(), Grave, G);
		const FTransform DT(FRotator(0.f, Boss->GetActorRotation().Yaw, 0.f), G);
		if (AWRGraveBossProp* Dug = Boss->GetWorld()->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), DT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Dug->Kind = TEXT("dug");
			Dug->State = 0;
			Dug->SetLifeSpan(GbT(Boss, TEXT("dugHours"), 8.f) * 150.f);
			Dug->FinishSpawning(DT);
		}
		return true;
	}
	const FVector Goal = Route[St.Route];
	if (FVector::Dist2D(Goal, Boss->GetActorLocation()) < 350.f)
	{
		++St.Route;
		if (St.Route >= Route.Num())
		{
			// Home: he sits down against the door till dusk.
			St.bOnRound = false;
			St.Route = -1;
		}
		return true;
	}
	Boss->SetHome(Boss->GetActorLocation());
	GbMoveTo(Boss, Goal, FMath::Clamp(GbT(Boss, TEXT("walkSpeed"), 170.f) / FMath::Max(100.f, Boss->GetCharacterMovement()->MaxWalkSpeed), 0.2f, 1.f));
	// The hounds keep 30 m behind him.
	if (T >= St.NextHoundStep)
	{
		St.NextHoundStep = T + 2.0;
		const FVector Behind = Boss->GetActorLocation() - Boss->GetActorForwardVector() * GbT(Boss, TEXT("houndTrail"), 3000.f);
		for (const TWeakObjectPtr<AWRCreature>& H : St.Hounds) if (H.IsValid() && H->IsAlive()) H->SetHome(Behind);
	}
	return true;
}

bool FWRGraveBoss::TickNhal(AWRBoss* Boss, FWRGraveBossState& St, float Dt)
{
	const double T = GbNow(Boss);
	// The grave-fire pools (decay 20 a second while you stand in one).
	for (int32 i = St.Pools.Num() - 1; i >= 0; --i)
	{
		FWRGraveBossState::FPool& P = St.Pools[i];
		if (T >= P.Until) { St.Pools.RemoveAt(i); continue; }
		if (T < P.TickAt) continue;
		P.TickAt = T + 0.5;
		for (AWRCharacterBase* C : GbFoes(Boss, GbT(Boss, TEXT("poolRadius"), 400.f), P.At))
		{
			if (FMath::Abs(GbFeet(C).Z - P.At.Z) < 200.f) GbSqueeze(Boss, C, 0.f, TEXT("decay"), GbT(Boss, TEXT("poolDecay"), 20.f) * 0.5f);
		}
	}
	// Phase 2: each open tomb raises a Risen every 25 s (one in four a bearer), at most 8 up; sealed tombs raise nothing.
	if (Boss->Phase >= 2 && T >= St.NextRaiseAt)
	{
		St.NextRaiseAt = T + GbT(Boss, TEXT("raiseEvery"), 25.f);
		const int32 Max = int32(GbT(Boss, TEXT("risenMax"), 8.f));
		for (const TWeakObjectPtr<AWRGraveBossProp>& Tb : St.Tombs)
		{
			if (!Tb.IsValid() || Tb->State != 1 || GbLive(St.Adds) >= Max) continue;
			Raise(Boss, St, FMath::RandRange(0, 3) == 0 ? FName(TEXT("risen_bearer")) : FName(TEXT("risen_mourner")), Tb->GetActorLocation() + FVector(0.f, 0.f, 50.f));
		}
	}
	// The Opening: forced the moment phase 2 begins and she is free (its row has no weight of its own).
	if (St.bOpeningDue && Boss->Current == INDEX_NONE && St.KneelUntil <= T)
	{
		AWRCharacterBase* Near = nullptr;
		float Best = 1e12f;
		for (AWRCharacterBase* C : GbFoes(Boss, St.ArenaR + 2000.f, Boss->GetActorLocation()))
		{
			const float D = float(FVector::DistSquared2D(C->GetActorLocation(), Boss->GetActorLocation()));
			if (D < Best) { Best = D; Near = C; }
		}
		if (Near && Force(Boss, TEXT("the_opening"), Near)) St.bOpeningDue = false;
	}
	// Kneeling as the Procession pours out of her.
	if (St.KneelUntil > T)
	{
		if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement()) Move->StopMovementImmediately();
		return true;
	}
	if (St.KneelUntil > 0.0)
	{
		St.KneelUntil = 0.0;
		if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(Boss)) Body->Pose = 0;
	}
	// The Procession: the bell tolls every 10 s and turns them back; a shade's swinging pole hurts (25, decay 30).
	if (AWRGraveBossProp* P = St.Procession.Get())
	{
		const float Now = P->ServerNow();
		if (T >= St.NextShadeToll)
		{
			St.NextShadeToll = T + GbT(Boss, TEXT("tollEvery"), 10.f);
			P->TurnAngle = P->RingAngle(Now);
			P->TurnAt = Now;
			P->Dir = -P->Dir;
			WRFx::Broadcast(Boss, TEXT("grave.toll_ring"), St.Home + FRotator(0.f, St.Yaw, 0.f).RotateVector(FVector(WRGraveChapel::TowerX, 0.f, 2400.f)), FRotator::ZeroRotator, 1.f, TEXT("grave.bell_toll"));
		}
		for (AWRCharacterBase* C : GbFoes(Boss, P->RingRadius(Now) * WRTune(TEXT("bosses.nhal.ringStretch"), 2.2f) + 300.f, P->GetActorLocation()))
		{
			if (!C->GetPlayerState() && C->GetTameOwner().IsEmpty()) continue;
			const double* Last = St.ShadeHitAt.Find(C);
			if (Last && T - *Last < 1.5) continue;
			for (int32 i = 0; i < AWRGraveBossProp::ShadeCount(); ++i)
			{
				if (FVector::Dist2D(P->ShadeAt(i, Now), C->GetActorLocation()) > 110.f + C->GetSimpleCollisionRadius()) continue;
				St.ShadeHitAt.Add(C, T);
				GbHitWith(Boss, C, GbT(Boss, TEXT("shadeDamage"), 25.f), 60.f, 3, false, false, P->ShadeAt(i, Now), TEXT("decay"), GbT(Boss, TEXT("shadeDecay"), 30.f));
				break;
			}
		}
	}
	(void)Dt;
	return false;
}

// ------------------------------------------------------------------------------------------------ the arenas' layout

bool FWRGraveBoss::ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw)
{
	const FWRGraveData& D = FWRGraveData::Get();
	if (!D.bLoaded) return false;
	Radius = WRTune(*FString::Printf(TEXT("bosses.%s.arenaRadius"), *BossId.ToString()), 3000.f);
	Yaw = 0.f;
	for (const FWRGraveData::FSite& S : D.Sites)
	{
		if (S.Arena == BossId) { Centre = S.At; Yaw = S.Yaw; return true; }
	}
	for (const FWRGraveData::FRoom& R : D.Rooms)
	{
		if (R.Arena == BossId) { Centre = R.At; Yaw = R.Yaw; Radius = FMath::Min(Radius, FMath::Max(R.W, R.L) * 0.5f); return true; }
	}
	if (BossId == TEXT("gravedigger") && !D.GravediggerLair.IsNearlyZero())
	{
		Centre = D.GravediggerLair;
		return true;
	}
	return false;
}
