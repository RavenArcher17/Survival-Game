// The Fungal caverns' bosses: their counters, own moves and the hooks AWRBoss calls (cooks/biome-fungal.md 2.13-2.14;
// cooks/build-biome-fungal.md 2.13-2.14). See WRFungalBosses.h. The things' and bodies' looks are WRFungalBossLook.cpp.
//
// Every blow here goes through UWRCombatLibrary::ApplyHit, and only where the body (or the thing it threw) really is:
// lobs fly a gravity arc from where they left and never home; a cloud is a real AWRSporeCloud (fire flashes it, wind
// pushes it, and pushed onto Mycelorn it chokes it).

#include "WRFungalBosses.h"
#include "WRFungalCreatures.h"
#include "WRFungalHazards.h"
#include "WRFungalTypes.h"
#include "WRFungalWorld.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

/** Server: one Fungal boss's fight. */
struct FWRFungalBossState
{
	FName Kind;
	bool bBegun = false;
	FVector Home = FVector::ZeroVector;
	float ArenaR = 3000.f, Yaw = 0.f;
	TArray<TWeakObjectPtr<AWRFungalBossProp>> Props;
	TArray<TWeakObjectPtr<AWRCreature>> Spawned;
	// A hold (the press, the engulf, the cocoon): who, until when, the next squeeze, what allies dealt to free them.
	TWeakObjectPtr<AWRCharacterBase> Held;
	double HeldUntil = 0.0, HeldTickAt = 0.0;
	float HeldFree = 0.f;
	FName HoldKind;
	// The Rot King.
	double NoHealUntil = 0.0, DryUntil = 0.0;
	// The Weaver.
	int32 RouteIndex = 0;
	double RestUntil = 0.0, NextWebAt = 0.0, NextEatAt = 0.0;
	FVector Settle = FVector::ZeroVector;
	// Mycelorn.
	double ChokeUntil = 0.0, GillsOffUntil = 0.0, NextChokeCheck = 0.0;
	float GillHP = 0.f;
	bool bUnder = false;
	double EruptAt = 0.0, SurfaceUntil = 0.0, NextPodBreathAt = 0.0, NextWholeAt = 0.0;
	int32 WholeStep = 0;
	FVector MoveTo = FVector::ZeroVector;
	double DraughtUntil[4] = { 0, 0, 0, 0 };
	TSet<TWeakObjectPtr<AWRSporeCloud>> Draughted[4];
	TWeakObjectPtr<AWRCreature> Summited;
	double SummitUntil = 0.0;
	FVector SummitFrom = FVector::ZeroVector;
	double NextSummitAt = 0.0;
};

namespace WRFungalBossImpl
{
	TMap<TWeakObjectPtr<const AWRBoss>, FWRFungalBossState>& FbStates() { static TMap<TWeakObjectPtr<const AWRBoss>, FWRFungalBossState> M; return M; }
	FWRFungalBossState& FbS(const AWRBoss* Boss) { return FbStates().FindOrAdd(Boss); }
	double FbNow(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }
	float FbT(const AWRBoss* Boss, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback); }
	float FbDamageScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }
	FName FbKindOf(const AWRBoss* Boss)
	{
		if (!Boss) return NAME_None;
		const FName Id = Boss->CreatureId;
		return Id == TEXT("sporemother") || Id == TEXT("rot_king") || Id == TEXT("weaver") || Id == TEXT("mycelorn") ? Id : NAME_None;
	}
	bool FbGround(const UWorld* World, const FVector& At, FVector& Out, float Up = 400.f, float Down = 2000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFungalBossGround), false);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}
	TArray<AWRCharacterBase*> FbFoes(const AWRBoss* Boss, float Range, const FVector& From)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It)) continue;
			if (FVector::Dist2D(It->GetActorLocation(), From) > Range + It->GetSimpleCollisionRadius() || FMath::Abs(It->GetActorLocation().Z - From.Z) > 1500.f) continue;
			Out.Add(*It);
		}
		return Out;
	}
	void FbTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** A blow that isn't the row's own strike (a squeeze, a tick of gas, a burst), with the boss's scaling. */
	void FbHit(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, bool bUnblockable, bool bLow, const FVector& Origin, FName Buildup = NAME_None, float Amount = 0.f, bool bMelee = true)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * FbDamageScale() * (Boss->Phase >= 3 ? FbT(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = Poise;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bMelee = bMelee;
		Hit.bLow = bLow;
		Hit.bHeavy = Damage >= 30.f;
		Hit.Tier = Damage >= 30.f ? EWRHitTier::Heavy : EWRHitTier::Light;
		Hit.Origin = Origin;
		Hit.Element = WRTags::Element_Physical;
		Hit.PhysType = 2;
		if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	float FbHealthFraction(const AWRBoss* Boss)
	{
		const UWRAttributeSet* A = Boss->GetAttributes();
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}
	FVector FbHitFrom(const AActor* Boss, const FWRHit& Hit)
	{
		if (!Hit.ImpactPoint.IsZero()) return Hit.ImpactPoint;
		if (!Hit.Origin.IsZero()) return Hit.Origin;
		return Hit.Instigator ? Hit.Instigator->GetActorLocation() : Boss->GetActorLocation();
	}
	FVector FbInside(const FWRFungalBossState& St, const FVector& P, float Margin)
	{
		const FVector D = P - St.Home;
		const float R = FMath::Max(100.f, St.ArenaR - Margin);
		return D.Size2D() <= R ? P : St.Home + D.GetSafeNormal2D() * R + FVector(0.f, 0.f, D.Z);
	}
	AWRFungalBossProp* FbSpawnProp(UWorld* World, FName Kind, const FVector& At, float Yaw, float Radius, int32 Index, AWRBoss* Owner)
	{
		const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
		AWRFungalBossProp* A = World->SpawnActorDeferred<AWRFungalBossProp>(AWRFungalBossProp::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) return nullptr;
		A->Kind = Kind;
		A->Radius = Radius;
		A->Index = Index;
		A->OwnerBoss = Owner;
		A->FinishSpawning(Tr);
		return A;
	}
	/** A Mycomite (or a Sporeling) out of a sac, an egg or the mat, coming for the nearest survivor. */
	AWRCreature* FbBrood(AWRBoss* Boss, FWRFungalBossState& St, FName Id, const FVector& At)
	{
		FVector G;
		FbGround(Boss->GetWorld(), At, G, 300.f, 1200.f);
		const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(Id);
		AWRCreature* C = AWRCreature::SpawnCreature(Boss->GetWorld(), Id, G + FVector(0.f, 0.f, (D ? D->HalfHeight : 30.f) + 20.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
		if (!C) return nullptr;
		AWRCharacterBase* Best = nullptr;
		float BestD = 1.e12f;
		for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 1500.f, St.Home)) { const float Dd = float(FVector::DistSquared(F->GetActorLocation(), G)); if (Dd < BestD) { BestD = Dd; Best = F; } }
		if (Best) C->SetAggroTarget(Best);
		C->SetHome(St.Home);
		St.Spawned.Add(C);
		return C;
	}
	int32 FbAlive(FWRFungalBossState& St, FName Id)
	{
		St.Spawned.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
		int32 N = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : St.Spawned) if (C->CreatureId == Id) ++N;
		return N;
	}
	/** A lob from a boss (an egg sac, a spore pod, a stalk) on a gravity arc, never homing; OnLanded runs where it lands. */
	AWRProjectile* FbLob(AWRBoss* Boss, const FVector& From, const FVector& To, float Speed, float Damage, float Splash, const FString& Model, TFunction<void(const FVector&)> Landed, FName Buildup = NAME_None, float Amount = 0.f)
	{
		FActorSpawnParameters Params;
		Params.Owner = Boss;
		Params.Instigator = Boss;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector V = AWRProjectile::LobVelocity(From, To, Speed, 1.f);
		AWRProjectile* P = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, V.Rotation(), Params);
		if (!P) return nullptr;
		FWRHit H;
		H.Instigator = Boss;
		H.Damage = Damage * FbDamageScale();
		H.PoiseDamage = Damage * 2.f;
		H.Impact = 2;
		H.bProjectile = true;
		H.Element = WRTags::Element_Physical;
		if (!Buildup.IsNone() && Amount > 0.f) H.Buildups.Add(Buildup, Amount);
		P->InitLob(H, V, 1.f, Splash, Model);
		P->OnLanded = MoveTemp(Landed);
		// The landing point shows the whole flight (a shadow on the floor).
		const float Flight = FVector::Dist2D(From, To) / FMath::Max(100.f, Speed);
		WRFx::Broadcast(Boss, TEXT("fungal.landShadow"), To, FRotator::ZeroRotator, FMath::Max(1.f, Splash / 150.f), NAME_None, Flight);
		return P;
	}
	/** Heal a boss by Amount (its pool's rot). */
	void FbHeal(AWRBoss* Boss, float Amount)
	{
		UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
		const UWRAttributeSet* A = Boss->GetAttributes();
		if (!ASC || !A || Amount <= 0.f) return;
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
	}
	void FbRelease(AWRBoss* Boss, FWRFungalBossState& St, bool bHaul)
	{
		AWRCharacterBase* V = St.Held.Get();
		const FName Kind = St.HoldKind;
		St.Held = nullptr;
		St.HoldKind = NAME_None;
		if (UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss)) Body->Pose = 0;
		if (!V) return;
		if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
		V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		if (Kind == TEXT("engulf") && Boss->CreatureId == TEXT("mycelorn"))
		{
			// The five Spores planted on you burst as it lets go (8 + rot 10 each).
			const int32 N = int32(FbT(Boss, TEXT("engulfSpores"), 5.f));
			WRFx::Broadcast(Boss, TEXT("fungal.miteBurst"), V->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("fungal.puff"));
			for (int32 i = 0; i < N; ++i) FbHit(Boss, V, FbT(Boss, TEXT("engulfBurst"), 8.f), 10.f, 1, true, false, V->GetActorLocation(), TEXT("rot"), FbT(Boss, TEXT("engulfRot"), 10.f), false);
		}
		if (Kind == TEXT("cocoon") && bHaul)
		{
			// Hauled up to the ceiling, then dropped 6 m.
			FHitResult Up;
			const FVector From = V->GetActorLocation();
			const float Drop = FbT(Boss, TEXT("cocoonDrop"), 600.f);
			float H = Drop;
			if (Boss->GetWorld()->LineTraceSingleByChannel(Up, From, From + FVector(0.f, 0.f, Drop), ECC_WorldStatic)) H = FMath::Max(0.f, Up.Distance - V->GetSimpleCollisionHalfHeight() - 20.f);
			V->SetActorLocation(From + FVector(0.f, 0.f, H), true);
			WRFx::Broadcast(Boss, TEXT("fungal.webbed"), V->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("fungal.web_tear"));
		}
	}
	void FbStartHold(AWRBoss* Boss, FWRFungalBossState& St, AWRCharacterBase* V, FName Kind, float Seconds)
	{
		if (!V || !V->IsAlive() || St.Held.IsValid() || FWRFungalBoss::HolderOf(V)) return;
		St.Held = V;
		St.HoldKind = Kind;
		St.HeldUntil = FbNow(Boss) + Seconds;
		St.HeldTickAt = FbNow(Boss) + 0.5;
		St.HeldFree = 0.f;
		if (V->Status) V->Status->Stagger(Seconds + 0.2f);
		if (UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss)) Body->Pose = Kind == TEXT("engulf") && Boss->CreatureId == TEXT("mycelorn") ? 3 : 1;
		FbTell(V, Kind == TEXT("press") ? TEXT("Pinned under her: an ally's blows on her abdomen will free you") : Kind == TEXT("cocoon") ? TEXT("Wrapped in silk: fire on you cuts you free")
			: TEXT("Held: an ally's blows on its head will free you"));
		UE_LOG(LogWildreach, Log, TEXT("FUNGAL boss %s holds %s (%s %.1f s)"), *Boss->GetDisplayLabel(), *V->GetName(), *Kind.ToString(), Seconds);
	}
}
using namespace WRFungalBossImpl;

// ================================================================================================ the hooks

bool FWRFungalBoss::IsFungalBoss(const AWRBoss* Boss)
{
	return !FbKindOf(Boss).IsNone();
}

bool FWRFungalBoss::ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw)
{
	const FWRFungalData& D = FWRFungalData::Get();
	for (const FWRFungalData::FHall& H : D.Halls)
	{
		if (H.Arena == BossId || H.Lair == BossId)
		{
			Centre = FVector(H.At.X, H.At.Y, H.StandZ());
			Radius = FMath::Max(H.Rx, H.Ry);
			Yaw = H.Yaw;
			if (BossId == TEXT("mycelorn") && !D.PoolAt.IsZero()) Centre = FVector(D.PoolAt.X, D.PoolAt.Y, H.StandZ());
			return true;
		}
	}
	return false;
}

AWRBoss* FWRFungalBoss::HolderOf(const AActor* Victim)
{
	if (!Victim) return nullptr;
	for (auto& KV : FbStates()) if (KV.Value.Held.Get() == Victim && KV.Key.IsValid()) return const_cast<AWRBoss*>(KV.Key.Get());
	return nullptr;
}

void FWRFungalBoss::Begin(AWRBoss* Boss)
{
	if (!IsFungalBoss(Boss)) return;
	UWRFungalBossBody::Attach(Boss);
	if (!Boss->HasAuthority()) return;
	FWRFungalBossState& St = FbS(Boss);
	St.Kind = FbKindOf(Boss);
	St.bBegun = true;
	if (!ArenaOf(St.Kind, St.Home, St.ArenaR, St.Yaw)) St.Home = Boss->GetActorLocation();
	St.ArenaR = FMath::Max(St.ArenaR, FbT(Boss, TEXT("arenaRadius"), 3000.f) * 0.8f);
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		AWRFungalBossProp* P = W.Get();
		if (!P || P->Kind == TEXT("web") || FVector::Dist2D(P->GetActorLocation(), St.Home) > St.ArenaR + 3000.f) continue;
		if ((St.Kind == TEXT("sporemother") && P->Kind == TEXT("sac")) || (St.Kind == TEXT("rot_king") && P->Kind == TEXT("pool")) || (St.Kind == TEXT("mycelorn") && (P->Kind == TEXT("plug") || P->Kind == TEXT("pod"))))
		{
			P->OwnerBoss = Boss;
			St.Props.AddUnique(P);
		}
	}
	St.GillHP = FbT(Boss, TEXT("gillHealth"), 1500.f);
	if (St.Kind == TEXT("weaver"))
	{
		const TArray<FVector>& R = FWRFungalData::Get().WeaverRoute;
		float Best = 1.e12f;
		for (int32 i = 0; i < R.Num(); ++i) { const float Dd = float(FVector::DistSquared(R[i], Boss->GetActorLocation())); if (Dd < Best) { Best = Dd; St.RouteIndex = i; } }
	}
	if (St.Kind == TEXT("mycelorn")) Boss->GetCharacterMovement()->MaxWalkSpeed = 0.f;
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL boss %s begins (arena %s r %.0f, %d things)"), *Boss->GetDisplayLabel(), *St.Home.ToCompactString(), St.ArenaR, St.Props.Num());
}

void FWRFungalBoss::Reset(AWRBoss* Boss)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRFungalBossState& St = FbS(Boss);
	FbRelease(Boss, St, false);
	for (const TWeakObjectPtr<AWRCreature>& C : St.Spawned) if (C.IsValid()) C->Destroy();
	St.Spawned.Reset();
	// The arena whole again: the sacs, the plugs; the spilled pools dry, the vats full; the pods and webs gone.
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : St.Props)
	{
		AWRFungalBossProp* P = W.Get();
		if (!P) continue;
		if (P->Kind == TEXT("pod") || (P->Kind == TEXT("pool") && !P->bPermanent)) { P->Destroy(); continue; }
		P->Health = P->Kind == TEXT("sac") ? FbT(Boss, TEXT("sacHealth"), 150.f) : P->Kind == TEXT("plug") ? FbT(Boss, TEXT("plugHealth"), 600.f) : 0.f;
		P->SetState(0);
		P->NextAt = FbNow(Boss) + FbT(Boss, TEXT("sacEvery"), 25.f);
	}
	St.Props.RemoveAll([](const TWeakObjectPtr<AWRFungalBossProp>& P) { return !P.IsValid() || P->IsActorBeingDestroyed(); });
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All()) if (W.IsValid() && W->Kind == TEXT("web") && W->OwnerBoss == Boss) W->Destroy();
	if (UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss)) { Body->Flags &= WRFungalBossFlag::Asleep; Body->Pose = 0; }
	St.bUnder = false;
	St.ChokeUntil = St.GillsOffUntil = St.NoHealUntil = St.DryUntil = 0.0;
	St.GillHP = FbT(Boss, TEXT("gillHealth"), 1500.f);
	if (AWRCreature* T = St.Summited.Get()) T->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	St.Summited = nullptr;
	Boss->SetActorHiddenInGame(false);
	Boss->SetActorEnableCollision(true);
	if (St.Kind == TEXT("mycelorn"))
	{
		// Back in its pool.
		FVector G;
		FbGround(Boss->GetWorld(), St.Home, G, 600.f, 2000.f);
		Boss->SetActorLocation(G + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void FWRFungalBoss::EndPlay(AWRBoss* Boss)
{
	if (FWRFungalBossState* St = FbStates().Find(Boss))
	{
		for (const TWeakObjectPtr<AWRCreature>& C : St->Spawned) if (C.IsValid()) C->Destroy();
	}
	FbStates().Remove(Boss);
}

void FWRFungalBoss::Died(AWRBoss* Boss)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRFungalBossState& St = FbS(Boss);
	FbRelease(Boss, St, false);
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All()) if (W.IsValid() && (W->Kind == TEXT("web") || W->Kind == TEXT("pod")) && W->OwnerBoss == Boss) W->Destroy();
	if (AWRCreature* T = St.Summited.Get()) T->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Boss->SetActorHiddenInGame(false);
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL boss %s falls"), *Boss->GetDisplayLabel());
}

bool FWRFungalBoss::OwnWeakness(const AWRBoss* Boss)
{
	return IsFungalBoss(Boss);
}

FName FWRFungalBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == TEXT("mycelorn")) return bScarred ? FName(TEXT("part_hive_spore_scarred")) : FName(TEXT("part_hive_spore"));
	return Fallback;
}

// ================================================================================================ idle: the Weaver's loop and sleep

bool FWRFungalBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRFungalBossState& St = FbS(Boss);
	if (!St.bBegun) Begin(Boss);
	if (St.Kind == TEXT("mycelorn"))
	{
		Boss->GetCharacterMovement()->StopMovementImmediately();
		return true;
	}
	if (St.Kind != TEXT("weaver")) return false;
	UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss);
	const double T = FbNow(Boss);
	const TArray<FVector>& R = FWRFungalData::Get().WeaverRoute;
	// By day she sleeps hanging in the False Sky (her lair's ceiling); a sneak blow opens the fight.
	if (!AWRGameState::IsNightIn(Boss->GetWorld()))
	{
		bSkipWake = true;
		if (Body && !Body->Has(WRFungalBossFlag::Asleep))
		{
			FVector Lair;
			float Rr = 0.f, Yw = 0.f;
			if (ArenaOf(TEXT("weaver"), Lair, Rr, Yw))
			{
				FHitResult Up;
				const FVector From = Lair + FVector(0.f, 0.f, 200.f);
				if (Boss->GetWorld()->LineTraceSingleByChannel(Up, From, From + FVector(0.f, 0.f, 4000.f), ECC_WorldStatic)) Lair = Up.ImpactPoint - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 30.f);
				Boss->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				Boss->SetActorLocation(Lair, false, nullptr, ETeleportType::TeleportPhysics);
				Boss->SetHome(Lair);
			}
			Body->Set(WRFungalBossFlag::Asleep, true);
		}
		Boss->GetCharacterMovement()->StopMovementImmediately();
		return true;
	}
	if (Body && Body->Has(WRFungalBossFlag::Asleep))
	{
		Body->Set(WRFungalBossFlag::Asleep, false);
		Boss->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	// The living world: she reels Glowgrubs and Caplings up into the starfield and kills a Sporestalker she meets.
	if (T >= St.NextEatAt)
	{
		St.NextEatAt = T + FbT(Boss, TEXT("eatEvery"), 60.f) * 0.25f;
		for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It; ++It)
		{
			AWRCreature* C = *It;
			if (C == Boss || C->IsTamed() || !C->IsAlive() || FVector::Dist(C->GetActorLocation(), Boss->GetActorLocation()) > 1500.f) continue;
			if (C->CreatureId == TEXT("glowgrub") || C->CreatureId == TEXT("capling") || C->CreatureId == TEXT("sporestalker"))
			{
				Boss->PlaySwing(EWRSwingKind::Heavy, 0.8f);
				WRFx::Broadcast(Boss, C->CreatureId == TEXT("sporestalker") ? TEXT("hit.blood") : TEXT("fungal.webbed"), C->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("fungal.line_reel"));
				C->Destroy();
				St.NextEatAt = T + FbT(Boss, TEXT("eatEvery"), 60.f);
				break;
			}
		}
	}
	if (R.Num() < 2) return false;
	// Her loop (fungal 2.13.3): the False Sky, the Sand Falls, the Lure Gallery, Knockers' Junction, the Shoestring
	// Bridge, the Silk Larders and back, at 2 m/s; she rests 4 min at the lair and webs where she rests.
	if (T < St.RestUntil)
	{
		if (T >= St.NextWebAt)
		{
			St.NextWebAt = T + FbT(Boss, TEXT("webEvery"), 10.f);
			int32 N = 0;
			for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All()) if (W.IsValid() && W->Kind == TEXT("web") && W->IsLive() && FVector::Dist2D(W->GetActorLocation(), Boss->GetActorLocation()) < FbT(Boss, TEXT("webRadius"), 1500.f) + 500.f) ++N;
			if (N < int32(FbT(Boss, TEXT("webMax"), 6.f)))
			{
				FVector G;
				const FVector Try = Boss->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(300.f, FbT(Boss, TEXT("webRadius"), 1500.f));
				if (FbGround(Boss->GetWorld(), Try, G, 400.f, 1200.f)) FbSpawnProp(Boss->GetWorld(), TEXT("web"), G, FMath::FRandRange(0.f, 180.f), FbT(Boss, TEXT("webSize"), 900.f), 0, Boss);
			}
		}
		return true;
	}
	const FVector Goal = R[St.RouteIndex];
	if (FVector::Dist2D(Goal, Boss->GetActorLocation()) < 300.f)
	{
		if (St.RouteIndex == 0) St.RestUntil = T + FbT(Boss, TEXT("restMinutes"), 4.f) * 60.f;
		St.RouteIndex = (St.RouteIndex + 1) % R.Num();
		return true;
	}
	Boss->SetHome(FVector(Goal.X, Goal.Y, Boss->GetActorLocation().Z));
	Boss->bReturning = false;
	const FWRCreatureDef* Def = Boss->GetDef();
	Boss->AddMovementInput((Goal - Boss->GetActorLocation()).GetSafeNormal2D(), FMath::Clamp(FbT(Boss, TEXT("walkSpeed"), 200.f) / FMath::Max(100.f, Def ? Def->Speed : 400.f), 0.1f, 1.f));
	return true;
}

// ================================================================================================ fighting

bool FWRFungalBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRFungalBossState& St = FbS(Boss);
	if (!St.bBegun) Begin(Boss);
	UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss);
	UWorld* World = Boss->GetWorld();
	const double T = FbNow(Boss);
	if (Body && Body->Has(WRFungalBossFlag::Asleep)) { Body->Set(WRFungalBossFlag::Asleep, false); Boss->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	// A hold under way: the victim kept in place, squeezed every 0.5 s; it ends on time (or an ally's blows free them).
	if (AWRCharacterBase* V = St.Held.Get())
	{
		if (!V->IsAlive() || T >= St.HeldUntil) FbRelease(Boss, St, St.HoldKind == TEXT("cocoon"));
		else
		{
			const FVector Front = Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() * 0.8f);
			FVector Keep = St.HoldKind == TEXT("press") ? FVector(Front.X, Front.Y, V->GetActorLocation().Z) : Front + FVector(0.f, 0.f, St.Kind == TEXT("mycelorn") ? 400.f : 0.f);
			if (St.Kind == TEXT("mycelorn")) V->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			V->SetActorLocation(Keep, true);
			if (T >= St.HeldTickAt)
			{
				St.HeldTickAt = T + 0.5;
				if (St.HoldKind == TEXT("press")) FbHit(Boss, V, FbT(Boss, TEXT("pressDps"), 20.f) * 0.5f, 0.f, 1, true, false, Boss->GetActorLocation());
				else if (St.HoldKind == TEXT("engulf") && St.Kind == TEXT("rot_king")) FbHit(Boss, V, FbT(Boss, TEXT("engulfDps"), 20.f) * 0.5f, 0.f, 1, true, false, Boss->GetActorLocation(), TEXT("rot"), FbT(Boss, TEXT("engulfRot"), 20.f) * 0.5f);
				else if (St.HoldKind == TEXT("engulf")) FbHit(Boss, V, FbT(Boss, TEXT("engulfDps"), 18.f) * 0.5f, 0.f, 1, true, false, Boss->GetActorLocation());
				else if (St.HoldKind == TEXT("cocoon") && V->Status) V->Status->AddStatusBuildup(WRStatus::Poison, FbT(Boss, TEXT("cocoonPoison"), 20.f) * 0.5f, Boss);
				if (V->Status) V->Status->Stagger(0.7f);
			}
		}
	}
	if (St.Kind == TEXT("sporemother"))
	{
		// The wall sacs hatch 3 Mycomites every 25 s while they stand (and she's fighting), up to a cap.
		for (const TWeakObjectPtr<AWRFungalBossProp>& W : St.Props)
		{
			AWRFungalBossProp* P = W.Get();
			if (!P || P->Kind != TEXT("sac") || !P->IsLive() || T < P->NextAt) continue;
			P->NextAt = T + FbT(Boss, TEXT("sacEvery"), 25.f);
			if (FbAlive(St, TEXT("mycomite")) >= int32(FbT(Boss, TEXT("mitesMax"), 24.f))) continue;
			WRFx::Broadcast(P, TEXT("fungal.eggHatch"), P->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.sporemother.egg"));
			for (int32 i = 0; i < int32(FbT(Boss, TEXT("sacHatch"), 3.f)); ++i) FbBrood(Boss, St, TEXT("mycomite"), P->GetActorLocation() + (St.Home - P->GetActorLocation()).GetSafeNormal2D() * 200.f + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), 0.f));
		}
	}
	if (St.Kind == TEXT("rot_king"))
	{
		// His pools: in one (live, not frozen) he heals 1.5% a second unless frostbite has stopped it; you take rot 15 a
		// second in one. Spilled pools dry after 20 s; frozen ones thaw after 15.
		if (Boss->HasTag(WRTags::Status_Frostbite)) St.NoHealUntil = FMath::Max(St.NoHealUntil, T + FbT(Boss, TEXT("frostbiteNoHeal"), 12.f));
		if (Body) Body->Set(WRFungalBossFlag::Dry, T < St.DryUntil);
		bool bIn = false;
		for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
		{
			AWRFungalBossProp* P = W.Get();
			if (!P || P->Kind != TEXT("pool") || FVector::Dist2D(P->GetActorLocation(), St.Home) > St.ArenaR + 2000.f) continue;
			if (P->State == 2 && T >= P->Until) P->SetState(0);
			if (!P->bPermanent && P->NextAt > 0.0 && T >= P->NextAt) { P->Destroy(); continue; }
			if (!P->IsLive()) continue;
			if (P->Contains(Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), Boss->GetSimpleCollisionRadius() * 0.5f)) bIn = true;
			for (AWRCharacterBase* F : FbFoes(Boss, P->Radius + 200.f, P->GetActorLocation()))
			{
				if (F->Status && P->Contains(F->GetActorLocation() - FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight()))) F->Status->AddStatusBuildup(WRStatus::Rot, FbT(Boss, TEXT("poolRot"), 15.f) * DeltaSeconds, Boss);
			}
		}
		if (bIn && T >= St.NoHealUntil && Boss->GetAttributes())
		{
			FbHeal(Boss, Boss->GetAttributes()->GetMaxHealth() * FbT(Boss, TEXT("healPct"), 0.015f) * DeltaSeconds);
		}
	}
	if (St.Kind == TEXT("weaver"))
	{
		// She webs the space where she fights: a sheet every 10 s up to six round her.
		if (T >= St.NextWebAt)
		{
			St.NextWebAt = T + FbT(Boss, TEXT("webEvery"), 10.f);
			if (St.Settle.IsZero()) St.Settle = Boss->GetActorLocation();
			int32 N = 0;
			for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All()) if (W.IsValid() && W->Kind == TEXT("web") && W->IsLive() && FVector::Dist2D(W->GetActorLocation(), St.Settle) < FbT(Boss, TEXT("webRadius"), 1500.f) + 600.f) ++N;
			if (N < int32(FbT(Boss, TEXT("webMax"), 6.f)))
			{
				FVector G;
				const FVector Try = Boss->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(200.f, FbT(Boss, TEXT("webRadius"), 1500.f));
				if (FbGround(World, Try, G, 400.f, 1200.f))
				{
					FbSpawnProp(World, TEXT("web"), G, FMath::FRandRange(0.f, 180.f), FbT(Boss, TEXT("webSize"), 900.f), 0, Boss);
					WRSound::PlayAt(Boss, TEXT("creature.weaver.spin"), G);
				}
			}
		}
	}
	if (St.Kind == TEXT("mycelorn")) return TickMycelorn(Boss, St, DeltaSeconds);
	return St.Held.IsValid();
}

namespace WRFungalBossImpl
{
	/** The veins' shortest path (the Physarum rule) from the node nearest A to the node nearest B: their points. */
	TArray<FVector> FbVeinPath(const FVector& A, const FVector& B)
	{
		const FWRFungalData& D = FWRFungalData::Get();
		TArray<FVector> Out;
		if (D.VeinNodes.Num() == 0) { Out.Add(A); Out.Add(B); return Out; }
		const auto Nearest = [&](const FVector& P) { int32 Best = 0; float BestD = 1.e12f; for (int32 i = 0; i < D.VeinNodes.Num(); ++i) { const float Dd = float(FVector::DistSquared2D(D.VeinNodes[i], P)); if (Dd < BestD) { BestD = Dd; Best = i; } } return Best; };
		const int32 S = Nearest(A), E = Nearest(B);
		TArray<float> Dist;
		TArray<int32> Prev;
		Dist.Init(1.e12f, D.VeinNodes.Num());
		Prev.Init(-1, D.VeinNodes.Num());
		TArray<bool> Done;
		Done.Init(false, D.VeinNodes.Num());
		Dist[S] = 0.f;
		for (int32 n = 0; n < D.VeinNodes.Num(); ++n)
		{
			int32 U = -1;
			for (int32 i = 0; i < D.VeinNodes.Num(); ++i) if (!Done[i] && (U < 0 || Dist[i] < Dist[U])) U = i;
			if (U < 0 || Dist[U] >= 1.e11f) break;
			Done[U] = true;
			if (U == E) break;
			for (const FIntPoint& Ed : D.VeinEdges)
			{
				const int32 V = Ed.X == U ? Ed.Y : Ed.Y == U ? Ed.X : -1;
				if (V < 0 || !D.VeinNodes.IsValidIndex(V)) continue;
				const float Nd = Dist[U] + float(FVector::Dist2D(D.VeinNodes[U], D.VeinNodes[V]));
				if (Nd < Dist[V]) { Dist[V] = Nd; Prev[V] = U; }
			}
		}
		Out.Add(A);
		TArray<FVector> Rev;
		for (int32 c = E; c >= 0; c = Prev[c]) { Rev.Add(D.VeinNodes[c]); if (c == S) break; }
		for (int32 i = Rev.Num() - 1; i >= 0; --i) Out.Add(Rev[i]);
		Out.Add(B);
		return Out;
	}
	/** Is P on one of the arena's raised terraces (it never surfaces there). */
	bool FbOnTerrace(const FVector& P)
	{
		for (const FWRFungalData::FAngled& Tr : FWRFungalData::Get().Terraces) if (FVector::Dist2D(Tr.At, P) < 700.f) return true;
		return false;
	}
	/** A cloud of Mycelorn's own (or its pods') right now. */
	bool FbOwnCloud(const AWRSporeCloud* C, const AWRBoss* Boss) { return C && C->IsLive() && C->Maker == Boss; }
}

bool FWRFungalBoss::TickMycelorn(AWRBoss* Boss, FWRFungalBossState& St, float Dt)
{
	UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss);
	UWorld* World = Boss->GetWorld();
	const double T = FbNow(Boss);
	Boss->GetCharacterMovement()->StopMovementImmediately();
	if (Body)
	{
		Body->Set(WRFungalBossFlag::Choked, T < St.ChokeUntil);
		Body->Set(WRFungalBossFlag::NoGills, T < St.GillsOffUntil);
		Body->Set(WRFungalBossFlag::Drained, Boss->Phase >= 2);
		Body->Set(WRFungalBossFlag::Split, Boss->Phase >= 3);
	}
	// The choke (fungal 2.14): its own cloud pushed onto it by wind in the last 2 s: the gills clench and go dark, a 4 s
	// stagger, its stance open (x2), and the cloud is spent.
	if (T >= St.NextChokeCheck && !St.bUnder)
	{
		St.NextChokeCheck = T + 0.2;
		for (const TWeakObjectPtr<AWRSporeCloud>& W : AWRSporeCloud::All())
		{
			AWRSporeCloud* C = W.Get();
			if (!FbOwnCloud(C, Boss) || T - C->LastWindAt > FbT(Boss, TEXT("chokeWindow"), 2.f)) continue;
			if (FVector::Dist2D(C->Centre(), Boss->GetActorLocation()) > C->RadiusNow() + Boss->GetSimpleCollisionRadius() + 200.f) continue;
			St.ChokeUntil = T + FbT(Boss, TEXT("chokeSeconds"), 4.f);
			Boss->StanceOpenUntil = St.ChokeUntil;
			if (Boss->Status) Boss->Status->Stagger(FbT(Boss, TEXT("chokeSeconds"), 4.f));
			WRFx::Broadcast(Boss, TEXT("fungal.choke"), Boss->GetActorLocation() + FVector(0.f, 0.f, 2000.f), FRotator::ZeroRotator, 3.f, TEXT("boss.mycelorn.choke"));
			for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 1000.f, St.Home)) FbTell(F, TEXT("Its own spores blown back on it: Mycelorn chokes"));
			UE_LOG(LogWildreach, Log, TEXT("FUNGAL Mycelorn chokes on a cloud pushed by %s"), C->LastWindBy ? *C->LastWindBy->GetName() : TEXT("?"));
			C->Destroy();
			break;
		}
	}
	// The Draught gates: a broken plug blows a 12 m lane of cold draught across the arena for 10 s: every cloud in it
	// pushed 15 m along it (once); the plug regrows after 45 s.
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : St.Props)
	{
		AWRFungalBossProp* P = W.Get();
		if (!P || P->Kind != TEXT("plug")) continue;
		const int32 g = FMath::Clamp(P->Index, 0, 3);
		if (P->State == 1 && T >= P->NextAt) { P->Health = FbT(Boss, TEXT("plugHealth"), 600.f); P->SetState(0); St.Draughted[g].Reset(); }
		if (T >= St.DraughtUntil[g]) continue;
		const FVector Dir = (St.Home - P->GetActorLocation()).GetSafeNormal2D();
		const FVector From = P->GetActorLocation();
		for (const TWeakObjectPtr<AWRSporeCloud>& Cw : AWRSporeCloud::All())
		{
			AWRSporeCloud* C = Cw.Get();
			if (!C || !C->IsLive() || St.Draughted[g].Contains(C)) continue;
			const FVector To = C->Centre() - From;
			const float Along = FVector::DotProduct(To, Dir);
			const float Across = FMath::Abs(FVector::DotProduct(To, FVector::CrossProduct(Dir, FVector::UpVector)));
			if (Along < 0.f || Along > St.ArenaR * 2.2f || Across > FbT(Boss, TEXT("draughtWidth"), 600.f) + C->RadiusNow() * 0.5f) continue;
			St.Draughted[g].Add(C);
			C->Push(Dir, FbT(Boss, TEXT("draughtPush"), 1500.f), P);
		}
	}
	// Phase 2: it travels under the mat along the veins and surfaces in a fairy ring for 8-14 s.
	if (Boss->Phase >= 2)
	{
		// The mound pods breathe: every 20 s the four nearest a survivor exhale a 6 m cloud (its own) for 10 s.
		if (T >= St.NextPodBreathAt)
		{
			St.NextPodBreathAt = T + FbT(Boss, TEXT("podEvery"), 20.f);
			TArray<AWRFungalBossProp*> Pods;
			for (const TWeakObjectPtr<AWRFungalBossProp>& W : St.Props) if (W.IsValid() && W->Kind == TEXT("pod") && W->IsLive()) Pods.Add(W.Get());
			const TArray<AWRCharacterBase*> Foes = FbFoes(Boss, St.ArenaR + 500.f, St.Home);
			if (Foes.Num() > 0)
			{
				const FVector Near = Foes[FMath::RandRange(0, Foes.Num() - 1)]->GetActorLocation();
				Pods.Sort([Near](const AWRFungalBossProp& A, const AWRFungalBossProp& B) { return FVector::DistSquared(A.GetActorLocation(), Near) < FVector::DistSquared(B.GetActorLocation(), Near); });
				for (int32 i = 0; i < FMath::Min(4, Pods.Num()); ++i)
				{
					AWRSporeCloud::Spawn(World, Pods[i]->GetActorLocation() + FVector(0.f, 0.f, 150.f), FbT(Boss, TEXT("podCloud"), 600.f), TEXT("boss"), FbT(Boss, TEXT("podCloudSeconds"), 10.f), Boss);
					WRFx::Broadcast(Pods[i], TEXT("fungal.podExhale"), Pods[i]->GetActorLocation() + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, 1.5f, TEXT("boss.mycelorn.pod"));
				}
			}
		}
		if (St.bUnder)
		{
			if (T >= St.EruptAt)
			{
				// Up through the ring: if one of its own clouds is over the spot it erupts into it and chokes 5 s.
				St.bUnder = false;
				Boss->SetActorLocation(St.MoveTo + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				Boss->SetActorHiddenInGame(false);
				Boss->SetActorEnableCollision(true);
				if (Body) Body->Set(WRFungalBossFlag::Under, false);
				St.SurfaceUntil = T + FMath::FRandRange(FbT(Boss, TEXT("surfaceMin"), 8.f), FbT(Boss, TEXT("surfaceMax"), 14.f));
				bool bChoke = false;
				for (const TWeakObjectPtr<AWRSporeCloud>& W : AWRSporeCloud::All()) if (FbOwnCloud(W.Get(), Boss) && W->Contains(St.MoveTo + FVector(0.f, 0.f, 150.f), 200.f)) { bChoke = true; W->Destroy(); break; }
				if (bChoke)
				{
					St.ChokeUntil = T + FbT(Boss, TEXT("eruptChoke"), 5.f);
					Boss->StanceOpenUntil = St.ChokeUntil;
					if (Boss->Status) Boss->Status->Stagger(FbT(Boss, TEXT("eruptChoke"), 5.f));
					WRFx::Broadcast(Boss, TEXT("fungal.choke"), Boss->GetActorLocation() + FVector(0.f, 0.f, 1500.f), FRotator::ZeroRotator, 3.f, TEXT("boss.mycelorn.choke"));
				}
				else if (AWRCharacterBase* Tgt = FbFoes(Boss, 3000.f, St.MoveTo).Num() > 0 ? FbFoes(Boss, 3000.f, St.MoveTo)[0] : nullptr)
				{
					const int32 i = Boss->Attacks.IndexOfByPredicate([](const FWRBossAttack& A) { return A.Id == TEXT("ring_eruption"); });
					if (Boss->Attacks.IsValidIndex(i) && Boss->Current == INDEX_NONE)
					{
						// The ring's spring already told it: the eruption's windup is spent under the mat.
						Boss->StartAttack(Boss->Attacks[i], Tgt);
					}
				}
			}
			return true;
		}
		if (T >= St.SurfaceUntil && Boss->Current == INDEX_NONE && !St.Held.IsValid() && T >= St.ChokeUntil)
		{
			// Down again: the veins race from here to where it will come up (never a terrace); a fairy ring springs
			// up there 1.2 s before.
			const TArray<AWRCharacterBase*> Foes = FbFoes(Boss, St.ArenaR + 500.f, St.Home);
			FVector Spot = St.Home;
			if (Foes.Num() > 0)
			{
				for (int32 Try = 0; Try < 12; ++Try)
				{
					const FVector P = FbInside(St, Foes[FMath::RandRange(0, Foes.Num() - 1)]->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(300.f, 700.f), 800.f);
					if (!FbOnTerrace(P)) { Spot = P; break; }
				}
			}
			FVector G;
			FbGround(World, Spot, G, 600.f, 2000.f);
			St.MoveTo = G;
			St.bUnder = true;
			Boss->SetActorHiddenInGame(true);
			Boss->SetActorEnableCollision(false);
			if (Body) Body->Set(WRFungalBossFlag::Under, true);
			WRFx::Broadcast(Boss, TEXT("fungal.sink"), Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 3.f, TEXT("boss.mycelorn.sink"));
			const TArray<FVector> Path = FbVeinPath(Boss->GetActorLocation(), G);
			float Along = 0.f;
			const float Speed = FbT(Boss, TEXT("veinSpeed"), 4000.f);
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			for (int32 i = 1; i < Path.Num(); ++i)
			{
				Along += float(FVector::Dist2D(Path[i - 1], Path[i]));
				const FVector A = Path[i - 1], B = Path[i];
				FTimerHandle H;
				World->GetTimerManager().SetTimer(H, [Weak, A, B]()
				{
					if (AWRBoss* Bs = Weak.Get()) WRFx::Broadcast(Bs, TEXT("fungal.veinPulse"), B, (B - A).Rotation(), FMath::Max(1.f, float(FVector::Dist2D(A, B)) / 600.f), NAME_None);
				}, FMath::Max(0.05f, Along / Speed), false);
			}
			const float Travel = FMath::Max(0.6f, Along / Speed);
			FTimerHandle Ring;
			World->GetTimerManager().SetTimer(Ring, [Weak, G]()
			{
				if (AWRBoss* Bs = Weak.Get()) WRFx::Broadcast(Bs, TEXT("fungal.fairyRing"), G, FRotator::ZeroRotator, 3.f, TEXT("boss.mycelorn.ring"));
			}, Travel, false);
			St.EruptAt = T + Travel + FbT(Boss, TEXT("ringTell"), 1.2f);
			UE_LOG(LogWildreach, Log, TEXT("FUNGAL Mycelorn travels the veins to %s (%.1f s)"), *G.ToCompactString(), Travel);
			return true;
		}
	}
	// Hidden phase 3: the Whole Cavern, one in turn every 8 s, each with its own tell.
	if (Boss->Phase >= 3 && T >= St.NextWholeAt && !St.bUnder)
	{
		St.NextWholeAt = T + FbT(Boss, TEXT("wholeEvery"), 8.f);
		const int32 Step = St.WholeStep++ % 4;
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		const TArray<AWRCharacterBase*> Foes = FbFoes(Boss, St.ArenaR + 500.f, St.Home);
		const FVector Home = St.Home;
		const float ArenaR = St.ArenaR;
		if (Step == 0)
		{
			// The ceiling drops lure lines over a third of the floor (the twang first): caught there, you're webbed.
			const float Sector = FMath::FRandRange(0.f, 360.f);
			for (int32 k = 0; k < 6; ++k) WRFx::Broadcast(Boss, TEXT("fungal.threadDrop"), Home + FRotator(0.f, Sector - 50.f + k * 20.f, 0.f).Vector() * ArenaR * FMath::FRandRange(0.3f, 0.85f), FRotator::ZeroRotator, 1.f, TEXT("fungal.line_twang"));
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [Weak, Sector, Home, ArenaR]()
			{
				AWRBoss* Bs = Weak.Get();
				if (!Bs) return;
				for (AWRCharacterBase* F : FbFoes(Bs, ArenaR + 300.f, Home))
				{
					const float A = (F->GetActorLocation() - Home).Rotation().Yaw;
					if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, Sector)) < 60.f) FWRFungalAI::Web(F, 1.5f, Bs);
				}
			}, 1.0f, false);
		}
		else if (Step == 1)
		{
			// The walls exhale: a hiss rising round the wall, then clouds in a 10 m band at their foot.
			WRSound::PlayAt(Boss, TEXT("boss.mycelorn.hiss"), Home);
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [Weak, Home, ArenaR]()
			{
				AWRBoss* Bs = Weak.Get();
				if (!Bs) return;
				for (int32 k = 0; k < 8; ++k)
				{
					FVector G;
					FbGround(Bs->GetWorld(), Home + FRotator(0.f, k * 45.f, 0.f).Vector() * ArenaR * 0.9f, G, 600.f, 2000.f);
					AWRSporeCloud::Spawn(Bs->GetWorld(), G + FVector(0.f, 0.f, 200.f), FbT(Bs, TEXT("wallCloud"), 1000.f) * 0.5f, TEXT("boss"), 10.f, Bs);
				}
			}, 1.2f, false);
		}
		else if (Step == 2)
		{
			// Cap-falls knock over three survivors and drop.
			for (int32 k = 0; k < FMath::Min(3, Foes.Num()); ++k)
			{
				const FVector At = Foes[k]->GetActorLocation();
				WRFx::Broadcast(Boss, TEXT("fungal.capFallMark"), At - FVector(0.f, 0.f, Foes[k]->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("fungal.cap_knock"));
				FTimerHandle H;
				World->GetTimerManager().SetTimer(H, [Weak, At]()
				{
					AWRBoss* Bs = Weak.Get();
					if (!Bs) return;
					WRFx::Broadcast(Bs, TEXT("fungal.capFallLand"), At, FRotator::ZeroRotator, 1.f, TEXT("fungal.cap_fall"));
					for (AWRCharacterBase* F : FbFoes(Bs, FbT(Bs, TEXT("capFallRadius"), 250.f), At)) FbHit(Bs, F, FbT(Bs, TEXT("capFallDamage"), 40.f), 80.f, 4, false, false, At + FVector(0.f, 0.f, 500.f), NAME_None, 0.f, false);
				}, 1.5f, false);
			}
		}
		else
		{
			// The elders on the terraces puff (their stalks' gills brighten first).
			for (const FWRFungalData::FAngled& Tr : FWRFungalData::Get().Terraces) WRFx::Broadcast(Boss, TEXT("fungal.flashTell"), Tr.At + FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, 1.f, TEXT("boss.mycelorn.elders"));
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [Weak]()
			{
				AWRBoss* Bs = Weak.Get();
				if (!Bs) return;
				for (const FWRFungalData::FAngled& Tr : FWRFungalData::Get().Terraces) AWRSporeCloud::Spawn(Bs->GetWorld(), Tr.At + FVector(0.f, 0.f, 300.f), FbT(Bs, TEXT("elderCloud"), 600.f), TEXT("boss"), 10.f, Bs);
			}, 1.0f, false);
		}
	}
	// The summited tame clamps on its terrace's wall 8 s, then drops back.
	if (AWRCreature* Tame = St.Summited.Get())
	{
		if (T >= St.SummitUntil || !Tame->IsAlive())
		{
			Tame->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Tame->SetCombatTag(WRFungalTags::Summited, false);
			St.Summited = nullptr;
		}
		else Tame->SetActorLocation(St.SummitFrom, false);
	}
	return St.Held.IsValid();
}

bool FWRFungalBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsFungalBoss(Boss)) return true;
	const FWRFungalBossState* St = FbStates().Find(Boss);
	const double T = FbNow(Boss);
	const FName Id = Attack.Id;
	if (St && (St->bUnder || St->Held.IsValid())) return false;
	if (Id == TEXT("ring_eruption") || Id == TEXT("sinking_root")) return false;   // forced only
	if (Boss->CreatureId == TEXT("mycelorn"))
	{
		if (Id == TEXT("gill_spores") && St && T < St->GillsOffUntil) return false;
		if (Id == TEXT("summit_call"))
		{
			if (FbHealthFraction(Boss) > FbT(Boss, TEXT("summitFrom"), 0.8f)) return false;
			bool bTame = false;
			for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It && !bTame; ++It) bTame = It->IsTamed() && It->IsAlive() && St && FVector::Dist2D(It->GetActorLocation(), St->Home) < St->ArenaR + 1000.f;
			if (!bTame) return false;
		}
		if (Id == TEXT("spore_mortar") && Target && FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) < FbT(Boss, TEXT("mortarMin"), 1400.f)) return false;
	}
	if (Boss->CreatureId == TEXT("weaver"))
	{
		// Off her web she's grounded: no Starfall drop; the volley's nets need 5 m.
		if (Id == TEXT("starfall_drop") && MoveScale(Boss) < 1.f) return false;
		if (Id == TEXT("web_volley") && Target && FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) < 500.f + Boss->GetSimpleCollisionRadius()) return false;
	}
	return true;
}

void FWRFungalBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return;
	UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss);
	const FName Id = Attack.Id;
	if (Body)
	{
		Body->Pose = Id == TEXT("club_slam") ? 1 : (Id.ToString().StartsWith(TEXT("lash")) || Id == TEXT("erupt_spin")) ? 2 : Id == TEXT("engulf") ? 3 : Id == TEXT("gill_spores") ? 4
			: (Id == TEXT("brood_press") || Id == TEXT("starfall_drop")) ? 1 : 0;
		Body->Set(WRFungalBossFlag::Rearing, Id == TEXT("sinking_root") || Id == TEXT("rot_spread") || Id == TEXT("brood_press"));
	}
	// Tells beyond the row's glint: the Sporemother's sac bulging to her vent, the Weaver's thread, the veins pulsing,
	// the crown pouring, the gills brightening.
	if (Id == TEXT("egg_toss")) WRFx::Broadcast(Boss, TEXT("fungal.eggBulge"), Boss->GetActorLocation() - Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius(), Boss->GetActorRotation(), 1.5f, TEXT("creature.sporemother.egg"));
	if (Id == TEXT("spore_vent")) WRFx::Broadcast(Boss, TEXT("fungal.flashTell"), Boss->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("boss.sporemother.vent"));
	if (Id == TEXT("rot_spread")) WRFx::Broadcast(Boss, TEXT("fungal.slurryPour"), Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 2.f), FRotator::ZeroRotator, 2.f, TEXT("boss.rot_king.pour"));
	if (Id == TEXT("starfall_drop") && Target)
	{
		// She climbs out of sight; a single bright thread drops beside you.
		Boss->SetActorHiddenInGame(true);
		Boss->SetActorEnableCollision(false);
		WRFx::Broadcast(Boss, TEXT("fungal.threadDrop"), Target->GetActorLocation() + Target->GetActorRightVector() * 120.f + FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.line_twang"));
	}
	if (Id == TEXT("sporeling_wave")) WRFx::Broadcast(Boss, TEXT("fungal.veinRings"), Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 6.f, TEXT("boss.mycelorn.veins"));
	if (Id == TEXT("sinking_root")) WRFx::Broadcast(Boss, TEXT("fungal.flashTell"), Boss->GetActorLocation() + FVector(0.f, 0.f, 2400.f), FRotator::ZeroRotator, 6.f, TEXT("boss.mycelorn.rear"));
	if (Id == TEXT("root_lash_line") && Target)
	{
		// That vein glows white end to end and the mat over it bulges.
		Attack.Range = FbT(Boss, TEXT("lashLine"), 3000.f);
		WRFx::Broadcast(Boss, TEXT("fungal.veinPulse"), Boss->GetActorLocation() + (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D() * 1500.f - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()),
			(Target->GetActorLocation() - Boss->GetActorLocation()).Rotation(), 5.f, TEXT("boss.mycelorn.bulge"));
	}
}

bool FWRFungalBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return false;
	FWRFungalBossState& St = FbS(Boss);
	UWorld* World = Boss->GetWorld();
	const FName Id = Attack.Id;
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	const FVector Feet = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
	const FVector Fwd = Boss->AttackFacing.Vector().GetSafeNormal2D();
	if (Id == TEXT("egg_toss"))
	{
		// Two sacs on 900 cm/s arcs to points 8-15 m off; each hatches 3 Mycomites 1.5 s after it lands (duds once
		// every wall sac has burnt).
		bool bLive = false;
		for (const TWeakObjectPtr<AWRFungalBossProp>& W : St.Props) bLive |= W.IsValid() && W->Kind == TEXT("sac") && W->IsLive();
		const FVector From = Boss->GetActorLocation() - Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
		for (int32 k = 0; k < 2; ++k)
		{
			const FVector Aim = Target ? Target->GetActorLocation() : Boss->GetActorLocation() + Fwd * 1000.f;
			FVector To = Boss->GetActorLocation() + ((Aim - Boss->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(k == 0 ? -25.f : 25.f, FVector::UpVector)) * FMath::FRandRange(FbT(Boss, TEXT("tossMin"), 800.f), FbT(Boss, TEXT("tossMax"), 1500.f));
			FbGround(World, FbInside(St, To, 300.f), To, 600.f, 2000.f);
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			FbLob(Boss, From, To, Attack.bRow ? Attack.Row.ProjectileSpeed : 900.f, 0.f, 0.f, TEXT("code:eggsac"), [Weak, bLive](const FVector& At)
			{
				AWRBoss* Bs = Weak.Get();
				if (!Bs) return;
				WRFx::Broadcast(Bs, TEXT("fungal.eggHatch"), At, FRotator::ZeroRotator, 1.f, bLive ? TEXT("creature.sporemother.egg") : TEXT("fungal.puff"));
				if (!bLive) return;
				FTimerHandle H;
				Bs->GetWorld()->GetTimerManager().SetTimer(H, [Weak, At]()
				{
					AWRBoss* B2 = Weak.Get();
					if (!B2 || !B2->IsAlive()) return;
					FWRFungalBossState& S2 = FbS(B2);
					for (int32 i = 0; i < int32(FbT(B2, TEXT("tossHatch"), 3.f)); ++i) FbBrood(B2, S2, TEXT("mycomite"), At + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f));
				}, FbT(Bs, TEXT("tossDelay"), 1.5f), false);
			});
		}
		return true;
	}
	if (Id == TEXT("spore_vent"))
	{
		AWRSporeCloud::Spawn(World, Boss->GetActorLocation(), FbT(Boss, TEXT("ventRadius"), 800.f), TEXT("boss"), FbT(Boss, TEXT("ventSeconds"), 6.f), Boss);
		WRFx::Broadcast(Boss, TEXT("fungal.sporeShake"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("boss.sporemother.vent"));
		return true;
	}
	if (Id == TEXT("rot_spread"))
	{
		// Painted behind the arc: an 8 m pool (none while his crown is dry).
		if (FbNow(Boss) >= St.DryUntil)
		{
			FVector G;
			FbGround(World, Boss->GetActorLocation() - Fwd * Boss->GetSimpleCollisionRadius() * 1.3f, G, 600.f, 1500.f);
			if (AWRFungalBossProp* P = FbSpawnProp(World, TEXT("pool"), G + FVector(0.f, 0.f, 3.f), 0.f, FbT(Boss, TEXT("spreadPool"), 800.f) * 0.5f, 0, Boss)) { P->NextAt = FbNow(Boss) + FbT(Boss, TEXT("poolSeconds"), 20.f); St.Props.Add(P); }
		}
		return true;
	}
	if (Id == TEXT("rot_belch"))
	{
		// A 10 m x 45 degree cone of rot gas, five ticks over 1.2 s.
		WRFx::Broadcast(Boss, TEXT("fungal.rotBelch"), Boss->GetActorLocation() + Fwd * Boss->GetSimpleCollisionRadius(), Boss->AttackFacing, 2.f, TEXT("boss.rot_king.belch"));
		const FVector At = Boss->GetActorLocation();
		const float Range = Attack.Range, Half = Attack.Size, Dmg = Attack.Damage;
		const float Rot = Attack.bRow && Attack.Row.Buildups.Contains(TEXT("rot")) ? Attack.Row.Buildups[TEXT("rot")] : 25.f;
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		const int32 Ticks = int32(FbT(Boss, TEXT("belchTicks"), 5.f));
		for (int32 k = 0; k < Ticks; ++k)
		{
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [Weak, At, Fwd, Range, Half, Dmg, Rot]()
			{
				AWRBoss* Bs = Weak.Get();
				if (!Bs || !Bs->IsAlive()) return;
				for (AWRCharacterBase* F : FbFoes(Bs, Range, At))
				{
					const FVector To = F->GetActorLocation() - At;
					if (FVector::DotProduct(To.GetSafeNormal2D(), Fwd) >= FMath::Cos(FMath::DegreesToRadians(Half))) FbHit(Bs, F, Dmg, 10.f, 1, true, false, At, TEXT("rot"), Rot, false);
				}
			}, 0.05f + k * 1.2f / FMath::Max(1, Ticks), false);
		}
		return true;
	}
	if (Id == TEXT("web_volley"))
	{
		// Three nets fanned 30 degrees apart at 1,000 cm/s, never homing: a hit webs you 2 s.
		const FVector From = Boss->GetActorLocation() - Fwd * Boss->GetSimpleCollisionRadius() * 0.5f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
		const FVector Aim = Boss->AttackPoint + FVector(0.f, 0.f, 80.f);
		const int32 N = int32(FbT(Boss, TEXT("volleyNets"), 3.f));
		for (int32 k = 0; k < N; ++k)
		{
			const float Off = (k - (N - 1) * 0.5f) * FbT(Boss, TEXT("volleyFan"), 30.f);
			const FVector Dir = (Aim - From).RotateAngleAxis(Off, FVector::UpVector);
			FActorSpawnParameters P;
			P.Owner = Boss;
			P.Instigator = Boss;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AWRProjectile* Net = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir.Rotation(), P);
			if (!Net) continue;
			FWRHit H;
			H.Instigator = Boss;
			H.Damage = Attack.Damage * FbDamageScale();
			H.PoiseDamage = Attack.Poise;
			H.Impact = 1;
			H.bProjectile = true;
			H.Element = WRTags::Element_Physical;
			Net->InitPhysical(H, AWRProjectile::LobVelocity(From, From + Dir, Attack.bRow ? Attack.Row.ProjectileSpeed : 1000.f, 0.3f), 0.3f, TEXT("code:webnet"), FString(), TEXT("webnet"), NAME_None);
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			Net->OnStruck = [Weak](AWRCharacterBase* V, EWRHitResult R) { if (V && (R == EWRHitResult::Hit || R == EWRHitResult::GuardBroken)) FWRFungalAI::Web(V, Weak.IsValid() ? FbT(Weak.Get(), TEXT("webbedSeconds"), 2.f) : 2.f, Weak.Get()); };
		}
		return true;
	}
	if (Id == TEXT("starfall_drop"))
	{
		// Down out of the dark onto the point (her own weight); the point's impact is the row's ring.
		Boss->SetActorLocation(Boss->AttackPoint + FVector(0.f, 0.f, FbT(Boss, TEXT("dropHeight"), 800.f)), false, nullptr, ETeleportType::TeleportPhysics);
		Boss->SetActorHiddenInGame(false);
		Boss->SetActorEnableCollision(true);
		Boss->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Boss->LaunchCharacter(FVector(0.f, 0.f, -1800.f), true, true);
		return false;
	}
	if (Id == TEXT("gill_spores"))
	{
		// The breath leaves its cloud where it passed (8 m, 20 s, its own: wind can turn it back on it).
		for (const float D : { 900.f, 1500.f })
		{
			FVector G;
			FbGround(World, Boss->GetActorLocation() + Fwd * D, G, 800.f, 2500.f);
			AWRSporeCloud::Spawn(World, G + FVector(0.f, 0.f, 200.f), FbT(Boss, TEXT("breathCloud"), 800.f), TEXT("boss"), FbT(Boss, TEXT("breathCloudSeconds"), 20.f), Boss);
		}
		WRFx::Broadcast(Boss, TEXT("fungal.gillBreath"), Boss->GetActorLocation() + FVector(0.f, 0.f, 1800.f) + Fwd * 600.f, Boss->AttackFacing, 4.f, TEXT("boss.mycelorn.breath"), Attack.bRow ? Attack.Row.Active : 1.4f);
		return false;   // the cone's own hits
	}
	if (Id == TEXT("spore_mortar"))
	{
		// Three puffball pods lobbed at survivors over 14 m away; each a 6 m cloud for 20 s.
		TArray<AWRCharacterBase*> Far;
		for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 1000.f, St.Home)) if (FVector::Dist2D(F->GetActorLocation(), Boss->GetActorLocation()) > FbT(Boss, TEXT("mortarMin"), 1400.f)) Far.Add(F);
		if (Far.Num() == 0 && Target) Far.Add(Target);
		const FVector From = Boss->GetActorLocation() + FVector(0.f, 0.f, 2600.f);
		for (int32 k = 0; k < 3 && Far.Num() > 0; ++k)
		{
			const AWRCharacterBase* F = Far[k % Far.Num()];
			FVector To = F->GetActorLocation() - FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight()) + (k >= Far.Num() ? FRotator(0.f, k * 120.f, 0.f).Vector() * 300.f : FVector::ZeroVector);
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			FbLob(Boss, From, To, Attack.bRow ? Attack.Row.ProjectileSpeed : 1000.f, Attack.Damage, 300.f, TEXT("code:sporepod"), [Weak](const FVector& At)
			{
				if (AWRBoss* Bs = Weak.Get()) AWRSporeCloud::Spawn(Bs->GetWorld(), At + FVector(0.f, 0.f, 150.f), FbT(Bs, TEXT("mortarCloud"), 600.f), TEXT("boss"), FbT(Bs, TEXT("mortarCloudSeconds"), 20.f), Bs);
			}, TEXT("poison"), Attack.bRow && Attack.Row.Buildups.Contains(TEXT("poison")) ? Attack.Row.Buildups[TEXT("poison")] : 30.f);
		}
		return true;
	}
	if (Id == TEXT("sporeling_wave"))
	{
		// Eight Sporelings rise from the mat at the arena's edge and rush.
		const int32 N = int32(FbT(Boss, TEXT("sporelings"), 8.f));
		for (int32 k = 0; k < N; ++k)
		{
			const FVector At = St.Home + FRotator(0.f, k * 360.f / N + FMath::FRandRange(-10.f, 10.f), 0.f).Vector() * St.ArenaR * 0.8f;
			FVector G;
			FbGround(World, At, G, 800.f, 2500.f);
			WRFx::Broadcast(Boss, TEXT("fungal.puff"), G, FRotator::ZeroRotator, 1.f, NAME_None);
			if (AWRCreature* C = FWRFungalAI::SpawnSporeling(World, G + FVector(0.f, 0.f, 60.f), nullptr))
			{
				AWRCharacterBase* Best = nullptr;
				float BestD = 1.e12f;
				for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 1500.f, St.Home)) { const float Dd = float(FVector::DistSquared(F->GetActorLocation(), G)); if (Dd < BestD) { BestD = Dd; Best = F; } }
				if (Best) C->SetAggroTarget(Best);
				St.Spawned.Add(C);
			}
		}
		return true;
	}
	if (Id == TEXT("summit_call"))
	{
		// A cordyceps stalk at the furthest tame (1,000 cm/s, never homing, seen the whole flight).
		AWRCreature* Far = nullptr;
		float FarD = -1.f;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (!It->IsTamed() || !It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), St.Home) > St.ArenaR + 1000.f) continue;
			const float Dd = float(FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation()));
			if (Dd > FarD) { FarD = Dd; Far = *It; }
		}
		if (!Far) return true;
		const FVector From = Boss->GetActorLocation() + FVector(0.f, 0.f, 2600.f);
		FActorSpawnParameters P;
		P.Owner = Boss;
		P.Instigator = Boss;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Stalk = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (Far->GetActorLocation() - From).Rotation(), P))
		{
			FWRHit H;
			H.Instigator = Boss;
			H.Damage = 1.f;
			H.bProjectile = true;
			H.Element = WRTags::Element_Physical;
			Stalk->InitPhysical(H, AWRProjectile::LobVelocity(From, Far->GetActorLocation(), 1000.f, 0.6f), 0.6f, TEXT("code:sporepellet"), FString(), TEXT("summit"), NAME_None);
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			Stalk->OnStruck = [Weak](AWRCharacterBase* V, EWRHitResult)
			{
				AWRBoss* Bs = Weak.Get();
				AWRCreature* Tame = Cast<AWRCreature>(V);
				if (!Bs || !Tame || !Tame->IsTamed()) return;
				FWRFungalBossState& S2 = FbS(Bs);
				// Summited: it walks to the nearest terrace's back wall, climbs, and clamps there 8 s, out of the fight.
				FVector Wall = Tame->GetActorLocation();
				float Best = 1.e12f;
				for (const FWRFungalData::FAngled& Tr : FWRFungalData::Get().Terraces)
				{
					const float Dd = float(FVector::DistSquared(Tr.At, Tame->GetActorLocation()));
					if (Dd < Best) { Best = Dd; Wall = Tr.At + (Tr.At - S2.Home).GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 400.f); }
				}
				S2.Summited = Tame;
				S2.SummitFrom = Wall;
				S2.SummitUntil = FbNow(Bs) + FbT(Bs, TEXT("summitSeconds"), 8.f);
				Tame->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				Tame->SetCombatTag(WRFungalTags::Summited, true);
				Tame->SetAggroTarget(nullptr);
				WRFx::Broadcast(Bs, TEXT("fungal.bloomBurst"), Tame->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("boss.mycelorn.summit"));
				UE_LOG(LogWildreach, Log, TEXT("FUNGAL Mycelorn summits %s"), *Tame->GetName());
			};
		}
		return true;
	}
	if (Id == TEXT("root_lash_line") && Target)
	{
		// Eight tendrils burst up along the vein toward you: 30 m x 2 m, unblockable.
		const FVector Dir = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		const FVector A = Feet, B = Feet + Dir * FbT(Boss, TEXT("lashLine"), 3000.f);
		for (int32 k = 0; k < 8; ++k) WRFx::Broadcast(Boss, TEXT("fungal.tendrilBurst"), FMath::Lerp(A, B, (k + 0.5f) / 8.f), Dir.Rotation(), 1.5f, k == 0 ? FName(TEXT("boss.mycelorn.lash")) : NAME_None);
		for (AWRCharacterBase* F : FbFoes(Boss, FbT(Boss, TEXT("lashLine"), 3000.f) + 200.f, Feet))
		{
			const FVector P = FMath::ClosestPointOnSegment(F->GetActorLocation(), A, B);
			if (FVector::Dist2D(P, F->GetActorLocation()) < 100.f + F->GetSimpleCollisionRadius()) FbHit(Boss, F, Attack.Damage, Attack.Poise, Attack.Impact, true, false, P, NAME_None, 0.f, true);
		}
		return true;
	}
	return false;
}

bool FWRFungalBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	(void)Boss; (void)Attack; (void)DeltaSeconds;
	return false;
}

void FWRFungalBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority()) return;
	FWRFungalBossState& St = FbS(Boss);
	UWRFungalBossBody* Body = UWRFungalBossBody::Get(Boss);
	if (Body && !St.Held.IsValid()) { Body->Pose = 0; Body->Set(WRFungalBossFlag::Rearing, false); }
	UWorld* World = Boss->GetWorld();
	const FName Id = Attack.Id;
	const FVector Fwd = Boss->GetActorForwardVector().GetSafeNormal2D();
	if (Id == TEXT("crown_slam") && FbNow(Boss) >= St.DryUntil)
	{
		// The slam spills a 6 m rot pool where it lands.
		FVector G;
		FbGround(World, Boss->GetActorLocation() + Fwd * Boss->GetSimpleCollisionRadius() * 1.2f, G, 600.f, 1500.f);
		if (AWRFungalBossProp* P = FbSpawnProp(World, TEXT("pool"), G + FVector(0.f, 0.f, 3.f), 0.f, FbT(Boss, TEXT("slamPool"), 600.f) * 0.5f, 0, Boss)) { P->NextAt = FbNow(Boss) + FbT(Boss, TEXT("poolSeconds"), 20.f); St.Props.Add(P); }
	}
	if (Id == TEXT("shell_spin"))
	{
		// It scatters the slurry: every pool within 6 m spreads 2 m.
		for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
		{
			if (W.IsValid() && W->Kind == TEXT("pool") && W->IsLive() && FVector::Dist2D(W->GetActorLocation(), Boss->GetActorLocation()) < FbT(Boss, TEXT("spinReach"), 600.f) + W->Radius)
			{
				W->Radius += FbT(Boss, TEXT("spinSpread"), 200.f);
				W->ForceNetUpdate();
			}
		}
	}
	if (Id == TEXT("club_slam")) WRFx::Broadcast(Boss, TEXT("fungal.veinPulse"), Boss->GetActorLocation() + Fwd * 1500.f - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), Fwd.Rotation(), 3.f, NAME_None);
	if (Id == TEXT("lash_3") && Boss->Phase >= 3)
	{
		// Phase 3: the Lash runs to four.
		const int32 i = Boss->Attacks.IndexOfByPredicate([](const FWRBossAttack& A) { return A.Id == TEXT("lash_4"); });
		if (Boss->Attacks.IsValidIndex(i) && Boss->AttackTarget.IsValid())
		{
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [Weak, i]() { AWRBoss* Bs = Weak.Get(); if (Bs && Bs->Current == INDEX_NONE && Bs->AttackTarget.IsValid() && Bs->Attacks.IsValidIndex(i)) Bs->StartAttack(Bs->Attacks[i], Bs->AttackTarget.Get()); }, 0.25f, false);
		}
	}
	if (Id == TEXT("sinking_root"))
	{
		// Gone into the floor: the pool drains, the veins brighten, the mound pods sprout; the travel begins.
		St.SurfaceUntil = FbNow(Boss) + 0.5;
		if (Body) Body->Set(WRFungalBossFlag::Drained, true);
	}
}

void FWRFungalBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	(void)Boss; (void)Attack; (void)Hit;
}

void FWRFungalBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsFungalBoss(Boss) || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (!bLanded) return;
	FWRFungalBossState& St = FbS(Boss);
	if (AttackId == TEXT("brood_press"))
	{
		FbStartHold(Boss, St, Victim, TEXT("press"), FbT(Boss, TEXT("pressSeconds"), 2.5f));
		// Four Mycomites latch on (one per limb).
		for (int32 k = 0; k < int32(FbT(Boss, TEXT("pressMites"), 4.f)); ++k)
		{
			if (AWRCreature* M = FbBrood(Boss, St, TEXT("mycomite"), Victim->GetActorLocation() + FRotator(0.f, k * 90.f, 0.f).Vector() * 120.f))
			{
				if (UWRFungalBody* B = UWRFungalBody::Get(M))
				{
					B->Victim = Victim;
					B->Set(WRFungalFlag::Latched, true);
					B->Until[2] = FbNow(Boss) + WRTune(TEXT("fungalLife.mycomite.latchSeconds"), 8.f);
					M->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				}
			}
		}
	}
	else if (AttackId == TEXT("engulf")) FbStartHold(Boss, St, Victim, TEXT("engulf"), FbT(Boss, TEXT("engulfSeconds"), 3.f));
	else if (AttackId == TEXT("cocoon")) FbStartHold(Boss, St, Victim, TEXT("cocoon"), FbT(Boss, TEXT("cocoonSeconds"), 3.f));
	else if (AttackId == TEXT("ring_eruption") && Victim->IsAlive()) Victim->LaunchCharacter(FVector(0.f, 0.f, 900.f), false, true);
}

float FWRFungalBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsFungalBoss(Boss) || Damage <= 0.f) return Damage;
	FWRFungalBossState& St = FbS(Boss);
	const double T = FbNow(Boss);
	const FVector At = FbHitFrom(Boss, Hit);
	const bool bWind = Hit.PathId == TEXT("wind");
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	// An ally's blows on the holder free the held: the Sporemother's abdomen, the Rot King's head, (100) the cocoon's
	// silk; a hold's own victim can't free themself this way.
	if (St.Held.IsValid() && By && By != St.Held.Get())
	{
		const FVector Local = Boss->GetActorRotation().UnrotateVector(At - Boss->GetActorLocation());
		const bool bOnPart = St.HoldKind == TEXT("press") ? Local.X < 0.f : St.HoldKind == TEXT("engulf") ? Local.X > 0.f : true;
		if (bOnPart)
		{
			St.HeldFree += Damage;
			const float Need = St.HoldKind == TEXT("press") ? FbT(Boss, TEXT("pressFree"), 150.f) : St.HoldKind == TEXT("cocoon") ? FbT(Boss, TEXT("cocoonFree"), 100.f) : FbT(Boss, TEXT("engulfFree"), 150.f);
			if (St.HeldFree >= Need) { FbTell(St.Held.Get(), TEXT("Freed")); FbRelease(Boss, St, false); }
		}
	}
	if (Boss->CreatureId == TEXT("rot_king") && bWind)
	{
		// A wind hit blows his rot aura off: the crown dries and stops dripping for 10 s.
		St.DryUntil = T + FbT(Boss, TEXT("drySeconds"), 10.f);
		WRFx::Broadcast(Boss, TEXT("fungal.cloudPush"), Boss->GetActorLocation() + FVector(0.f, 0.f, 300.f), FRotator::ZeroRotator, 2.f, TEXT("boss.rot_king.dry"));
	}
	if (Boss->CreatureId == TEXT("mycelorn"))
	{
		if (St.bUnder) return 0.f;
		// Weak to Wind (x1.5); the Heart-seed shown in phase 3 (x1.25); the gills (high on it) have their own 1,500.
		if (bWind) Damage *= FbT(Boss, TEXT("windWeak"), 1.5f) * (T < St.ChokeUntil ? FbT(Boss, TEXT("chokeWind"), 1.5f) : 1.f);
		if (Boss->Phase >= 3) Damage *= FbT(Boss, TEXT("heartTaken"), 1.25f);
		const float Base = Boss->GetActorLocation().Z - Boss->GetSimpleCollisionHalfHeight();
		if (At.Z > Base + 2000.f && T >= St.GillsOffUntil)
		{
			St.GillHP -= Damage;
			if (St.GillHP <= 0.f)
			{
				St.GillsOffUntil = T + FbT(Boss, TEXT("gillsOff"), 30.f);
				St.GillHP = FbT(Boss, TEXT("gillHealth"), 1500.f);
				WRFx::Broadcast(Boss, TEXT("fungal.bloomBurst"), Boss->GetActorLocation() + FVector(0.f, 0.f, 2400.f), FRotator::ZeroRotator, 4.f, TEXT("boss.mycelorn.gills"));
				for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 500.f, St.Home)) FbTell(F, TEXT("Its gills tear and go dark: no spore breath for a while"));
			}
		}
	}
	return Damage;
}

void FWRFungalBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	(void)Boss; (void)Hit; (void)Damage;
}

void FWRFungalBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsFungalBoss(Boss) || !Boss->HasAuthority() || Boss->CreatureId != TEXT("mycelorn")) return;
	FWRFungalBossState& St = FbS(Boss);
	UWorld* World = Boss->GetWorld();
	if (NewPhase == 2)
	{
		// Twelve mound pods sprout round the floor: its breathing pores.
		const TArray<FWRFungalData::FAngled>& Pods = FWRFungalData::Get().Pods;
		for (const FWRFungalData::FAngled& P : Pods)
		{
			FVector G;
			FbGround(World, P.At, G, 800.f, 2500.f);
			if (AWRFungalBossProp* Pr = FbSpawnProp(World, TEXT("pod"), G, FMath::FRandRange(0.f, 360.f), 100.f, P.I, Boss)) { Pr->Health = FbT(Boss, TEXT("podHealth"), 200.f); St.Props.Add(Pr); }
		}
		St.NextPodBreathAt = FbNow(Boss) + 6.0;
		for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 500.f, St.Home)) FbTell(F, TEXT("It sinks into the mat: watch the veins, and never stand in the ring"));
	}
	if (NewPhase >= 3)
	{
		St.NextWholeAt = FbNow(Boss) + 3.0;
		for (AWRCharacterBase* F : FbFoes(Boss, St.ArenaR + 500.f, St.Home)) FbTell(F, TEXT("The whole cavern wakes: its stem splits on the Heart-seed"));
	}
}

bool FWRFungalBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!IsFungalBoss(Boss)) return false;
	if (Boss->CreatureId == TEXT("weaver")) return true;   // she roams: no stone
	bool bAny = false;
	for (const FWRFungalData::FRunback& R : FWRFungalData::Get().Runback)
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
		if (!FbGround(Boss->GetWorld(), R.At, G, 300.f, 1500.f)) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Face(0.f, (Boss->GetHome() - G).Rotation().Yaw, 0.f);
		if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), G, Face, Params))
		{
			Stone->BossId = Boss->CreatureId;
			if (!Boss->Runback.IsValid()) Boss->Runback = Stone;
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s shrine-stone at %s"), *Boss->GetDisplayLabel(), *G.ToCompactString());
		}
	}
	return bAny;
}

// ================================================================================================ fire, frost, wind on the things

bool FWRFungalBoss::HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire)
{
	AWRFungalBossProp* P = Cast<AWRFungalBossProp>(Thing);
	if (!P || !P->HasAuthority() || Cast<AWRBoss>(By)) return false;
	(void)Comp;
	return P->TakeHit(By, Damage, bFire, false);
}

void FWRFungalBoss::FireAt(AActor* By, const FVector& At, float Radius)
{
	if (!By || !By->HasAuthority() || Cast<AWRBoss>(By)) return;
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		AWRFungalBossProp* P = W.Get();
		if (!P || !P->IsLive() || P->Kind == TEXT("pool")) continue;
		const float Reach = P->Kind == TEXT("web") ? FMath::Max(Radius, WRTune(TEXT("bosses.weaver.fireClear"), 400.f)) + P->Radius * 0.5f : FMath::Max(Radius, 200.f) + 150.f;
		if (FVector::Dist(P->GetActorLocation(), At) < Reach) P->TakeHit(By, 0.f, true, false);
	}
	// Fire on a cocoon cuts the wrapped free.
	for (auto& KV : FbStates())
	{
		FWRFungalBossState& St = KV.Value;
		if (St.HoldKind == TEXT("cocoon") && St.Held.IsValid() && KV.Key.IsValid() && FVector::Dist(St.Held->GetActorLocation(), At) < FMath::Max(Radius, 300.f) + 100.f)
		{
			FbTell(St.Held.Get(), TEXT("The fire eats the silk: you're free"));
			FbRelease(const_cast<AWRBoss*>(KV.Key.Get()), St, false);
		}
	}
}

void FWRFungalBoss::FrostAt(AActor* By, const FVector& At)
{
	if (!By || !By->HasAuthority() || Cast<AWRBoss>(By)) return;
	// Frost on a rot pool freezes it 15 s: no heal, no buildup, walkable ice.
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		AWRFungalBossProp* P = W.Get();
		if (!P || P->Kind != TEXT("pool") || P->State != 0 || !P->Contains(At, 300.f)) continue;
		P->Until = P->ServerNow() + WRTune(TEXT("bosses.rot_king.freezeSeconds"), 15.f);
		P->SetState(2);
		WRFx::Broadcast(P, TEXT("frost.impact"), P->GetActorLocation(), FRotator::ZeroRotator, P->Radius / 150.f, TEXT("frost.impact"));
	}
}

void FWRFungalBoss::Wind(AActor* By, const FVector& Origin, const FVector& Dir, float Range, float Cone, bool bStrong)
{
	if (!By || !By->HasAuthority() || Cast<AWRBoss>(By)) return;
	// Wind tears web: 8 m of it a gust (a Gale flask's 12).
	const float Reach = FMath::Max(Range, WRTune(TEXT("bosses.weaver.windClear"), 800.f)) * (bStrong ? 1.f : 0.8f);
	const FVector D = Dir.GetSafeNormal();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Cone, 10.f, 180.f) * 0.5f));
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		AWRFungalBossProp* P = W.Get();
		if (!P || P->Kind != TEXT("web") || !P->IsLive()) continue;
		const FVector To = P->GetActorLocation() + FVector(0.f, 0.f, 200.f) - Origin;
		const float Dist = To.Size();
		if (Dist > Reach + P->Radius * 0.5f || (Dist > 300.f && FVector::DotProduct(To / Dist, D) < CosHalf)) continue;
		P->SetState(1);
		WRFx::Broadcast(P, TEXT("fungal.webFall"), P->GetActorLocation() + FVector(0.f, 0.f, 300.f), P->GetActorRotation(), 1.f, TEXT("fungal.web_tear"));
	}
}

void FWRFungalBoss::CloudFlashed(AWRSporeCloud* Cloud, const TArray<AWRCharacterBase*>& Inside)
{
	if (!Cloud) return;
	// Webs inside the flash burn.
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		AWRFungalBossProp* P = W.Get();
		if (P && P->IsLive() && (P->Kind == TEXT("web") || P->Kind == TEXT("sac") || P->Kind == TEXT("pod")) && Cloud->Contains(P->GetActorLocation() + FVector(0.f, 0.f, 150.f), P->Radius)) P->TakeHit(Cloud, 0.f, true, false);
	}
	// The Sporemother's own vent cloud lit with her in it: 200 and a 3 s stagger.
	AWRBoss* Boss = Cast<AWRBoss>(Cloud->Maker);
	if (!Boss || Boss->CreatureId != TEXT("sporemother") || !Inside.Contains(Boss)) return;
	FWRHit H;
	H.Instigator = Cloud;
	H.Damage = FbT(Boss, TEXT("ventFlashDamage"), 200.f);
	H.Element = WRTags::Element_Fire;
	H.Impact = 5;
	H.PoiseDamage = 200.f;
	H.bUnblockable = true;
	H.bMelee = false;
	H.Origin = Cloud->Centre();
	UWRCombatLibrary::ApplyHit(Boss, H);
	if (Boss->Status) Boss->Status->Stagger(FbT(Boss, TEXT("ventFlashStagger"), 3.f));
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL the Sporemother's vent flashes on her"));
}

float FWRFungalBoss::MoveScale(const AActor* Who)
{
	const AWRBoss* Boss = Cast<AWRBoss>(Who);
	if (!Boss || Boss->CreatureId != TEXT("weaver")) return 1.f;
	for (const TWeakObjectPtr<AWRFungalBossProp>& W : AWRFungalBossProp::All())
	{
		if (W.IsValid() && W->Kind == TEXT("web") && W->IsLive() && FVector::Dist2D(W->GetActorLocation(), Boss->GetActorLocation()) < WRTune(TEXT("bosses.weaver.webNear"), 900.f) + W->Radius * 0.5f) return WRTune(TEXT("bosses.weaver.onWeb"), 1.6f);
	}
	return WRTune(TEXT("bosses.weaver.offWeb"), 0.6f);
}

// ================================================================================================ the things (server)

bool AWRFungalBossProp::TakeHit(AActor* By, float Damage, bool bFire, bool bBlunt)
{
	if (!HasAuthority() || !IsLive()) return Kind != TEXT("pool");
	AWRBoss* Boss = OwnerBoss.Get();
	if (Kind == TEXT("pool")) return false;
	if (Kind == TEXT("web"))
	{
		// Fire burns it; a blade cuts it (a hit of 60).
		if (bFire || Damage >= 60.f)
		{
			SetState(1);
			WRFx::Broadcast(this, bFire ? TEXT("fungal.webBurn") : TEXT("fungal.webFall"), GetActorLocation() + FVector(0.f, 0.f, 300.f), GetActorRotation(), 1.f, bFire ? TEXT("fungal.web_burn") : TEXT("fungal.web_tear"));
		}
		return true;
	}
	if (Kind == TEXT("sac"))
	{
		// Any fire hit kills a sac at once; else 150 HP (x3 fire).
		Health -= bFire ? 1.e6f : Damage;
		WRSound::PlayAt(this, TEXT("creature.sporemother.egg"), GetActorLocation(), 0.6f);
		if (Health > 0.f) return true;
		SetState(1);
		WRFx::Broadcast(this, bFire ? TEXT("fungal.webBurn") : TEXT("fungal.eggHatch"), GetActorLocation(), FRotator::ZeroRotator, 1.f, bFire ? TEXT("fungal.web_burn") : TEXT("fungal.puff"));
		// Each burnt sac makes her shriek and stagger 2 s within 20 m.
		if (Boss && Boss->IsAlive() && FVector::Dist(Boss->GetActorLocation(), GetActorLocation()) < WRTune(TEXT("bosses.sporemother.sacShriek"), 2000.f))
		{
			if (Boss->Status) Boss->Status->Stagger(WRTune(TEXT("bosses.sporemother.sacStagger"), 2.f));
			WRSound::Broadcast(Boss, TEXT("boss.sporemother.shriek"));
		}
		UE_LOG(LogWildreach, Log, TEXT("FUNGAL a brood sac burns (%s)"), By ? *By->GetName() : TEXT("?"));
		return true;
	}
	if (Kind == TEXT("pod"))
	{
		// Burnt by fire or broken by a hit of 200: gone for the fight.
		Health -= bFire ? 1.e6f : Damage;
		if (Health > 0.f && Damage < WRTune(TEXT("bosses.mycelorn.podHealth"), 200.f)) return true;
		SetState(1);
		WRFx::Broadcast(this, bFire ? TEXT("fungal.webBurn") : TEXT("fungal.rubbleBreak"), GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.puff"));
		return true;
	}
	if (Kind == TEXT("plug"))
	{
		// 600 HP, fire and blunt x2: broken, the draught blows its lane 10 s; it regrows in 45 s.
		Health -= FMath::Max(Damage, bFire ? 60.f : 0.f) * (bFire ? WRTune(TEXT("bosses.mycelorn.plugFire"), 2.f) : 1.f) * (bBlunt ? WRTune(TEXT("bosses.mycelorn.plugBlunt"), 2.f) : 1.f);
		WRSound::PlayAt(this, TEXT("fungal.cap_knock"), GetActorLocation(), 0.6f);
		if (Health > 0.f) return true;
		SetState(1);
		NextAt = ServerNow() + WRTune(TEXT("bosses.mycelorn.plugRegrow"), 45.f);
		if (Boss)
		{
			FWRFungalBossState& St = FbS(Boss);
			St.DraughtUntil[FMath::Clamp(Index, 0, 3)] = FbNow(Boss) + WRTune(TEXT("bosses.mycelorn.draughtSeconds"), 10.f);
			St.Draughted[FMath::Clamp(Index, 0, 3)].Reset();
		}
		WRFx::Broadcast(this, TEXT("fungal.draught"), GetActorLocation() + FVector(0.f, 0.f, 300.f), GetActorRotation(), 3.f, TEXT("fungal.draught"), WRTune(TEXT("bosses.mycelorn.draughtSeconds"), 10.f));
		UE_LOG(LogWildreach, Log, TEXT("FUNGAL a Draught gate's plug breaks"));
		return true;
	}
	return false;
}

// ================================================================================================ UWRFungalArenas

bool UWRFungalArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRFungalArenas::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && WRFungal::Live() && FWRFungalData::Get().bLoaded;
}

void UWRFungalArenas::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	PlaceClock += DeltaTime;
	if (PlaceClock < 2.f) return;
	PlaceClock = 0.f;
	const UWRFungalWorld* W = UWRFungalWorld::Get(World);
	if (!W || !W->IsBuilt()) return;
	static const FName Ids[] = { TEXT("sporemother"), TEXT("rot_king"), TEXT("weaver"), TEXT("mycelorn") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		if (Place(Id)) Placed.Add(Id);
	}
}

bool UWRFungalArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	FVector Centre;
	float Radius = 0.f, Yaw = 0.f;
	if (!FWRFungalBoss::ArenaOf(BossId, Centre, Radius, Yaw) || !FWRCreatureRegistry::Get().Find(BossId)) return true;
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Centre) < WRTune(TEXT("bosses.fungal.placeRadius"), 30000.f);
	if (!bNear) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It) if (It->CreatureId == BossId) return true;
	FVector G;
	if (!FbGround(World, Centre + FVector(0.f, 0.f, 100.f), G, 400.f, 2000.f)) return false;
	const FWRFungalData& D = FWRFungalData::Get();
	if (BossId == TEXT("sporemother"))
	{
		// The six egg sacs on the Brood Doline's walls, 2-6 m up.
		for (const FWRFungalData::FAngled& S : D.Sacs) if (AWRFungalBossProp* P = FbSpawnProp(World, TEXT("sac"), S.At, S.Angle + 180.f, 75.f, S.I, nullptr)) P->Health = WRTune(TEXT("bosses.sporemother.sacHealth"), 150.f);
	}
	if (BossId == TEXT("rot_king"))
	{
		// The Steeping Vats: the full ones are his pools (they never dry), the empty ones stand as stone.
		for (const FWRFungalData::FAngled& V : D.Vats)
		{
			if (!V.bFull) continue;
			FVector VG;
			FbGround(World, V.At + FVector(0.f, 0.f, 200.f), VG, 300.f, 800.f);
			if (AWRFungalBossProp* P = FbSpawnProp(World, TEXT("pool"), VG + FVector(0.f, 0.f, 3.f), 0.f, FMath::Max(150.f, V.R), V.I, nullptr)) P->bPermanent = true;
		}
	}
	if (BossId == TEXT("mycelorn"))
	{
		// The four Draught gates' plugs; the Spore path's altar on the south terrace.
		int32 g = 0;
		for (const FWRFungalData::FAngled& Gt : D.Gates)
		{
			FVector GG;
			FbGround(World, Gt.At + FVector(0.f, 0.f, 200.f), GG, 400.f, 1500.f);
			if (AWRFungalBossProp* P = FbSpawnProp(World, TEXT("plug"), GG, Gt.Angle, 300.f, g++, nullptr)) P->Health = WRTune(TEXT("bosses.mycelorn.plugHealth"), 600.f);
		}
		if (!D.AltarAt.IsZero())
		{
			FVector AG;
			FbGround(World, D.AltarAt + FVector(0.f, 0.f, 300.f), AG, 400.f, 1500.f);
			const FTransform AT(FRotator(0.f, (Centre - AG).Rotation().Yaw, 0.f), AG + FVector(0.f, 0.f, 70.f));
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), AT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("spore");
				Altar->FinishSpawning(AT);
				Altar->SetActorHiddenInGame(true);   // the dressing's stone basin is its look
			}
		}
	}
	// The summoning stones on the caverns' floors (beside the run-back stone's place; the Weaver's at her lair).
	{
		FVector Spot = FVector::ZeroVector, Toward = Centre;
		for (const FWRFungalData::FRunback& R : D.Runback) if (R.Boss == BossId) Spot = R.At;
		if (Spot.IsZero()) Spot = Centre + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * Radius * 0.6f;
		const FVector Side = FVector::CrossProduct((Toward - Spot).GetSafeNormal2D(), FVector::UpVector);
		FVector SG;
		if (FbGround(World, Spot + Side * WRTune(TEXT("world.summoning.sideOffset"), 600.f) + FVector(0.f, 0.f, 200.f), SG, 300.f, 1500.f))
		{
			bool bHave = false;
			for (TActorIterator<AWRSummoningStone> It(World); It && !bHave; ++It) bHave = It->BossId == BossId;
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
			if (!bHave && Def)
			{
				const FTransform ST(FRotator(0.f, (Toward - SG).Rotation().Yaw, 0.f), SG);
				if (AWRSummoningStone* S = World->SpawnActorDeferred<AWRSummoningStone>(AWRSummoningStone::StaticClass(), ST, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					S->BossId = BossId;
					S->Biome = Def->Biome;
					S->FinishSpawning(ST);
				}
			}
		}
	}
	FVector Start = G;
	const FTransform Where(FRotator(0.f, Yaw + 180.f, 0.f), Start + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = false;
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}
