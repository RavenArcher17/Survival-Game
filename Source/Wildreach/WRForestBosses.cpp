// The Ancient forest's bosses: their counter mechanics, moves and the hooks AWRBoss calls (cooks/biome-forest.md 2.10,
// 2.13, 2.14; cooks/build-biome-forest.md 2.10). See WRForestBosses.h. The arenas' pieces, scenery and placement are
// WRForestArenas.cpp; the bosses' code-built looks WRForestBossLook.cpp.
//
// Every blow here goes through UWRCombatLibrary::ApplyHit, and only where the body (or the thing it threw) really is:
// sweeps travel, lobs fly a gravity arc from a replicated launch and never home, ground waves roll out from the slam.
// Unblockable moves (grabs, the Uprooting, the root hand) wind up 0.9 s or more with a crimson glint.
#include "WRForestBosses.h"
#include "WRForestTypes.h"
#include "WRForestCreatures.h"
#include "WRForestHazards.h"
#include "WRBoss.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlantSummon.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"

/** A ground wave rolling out from a slam (Sylvaroth's hammer fists): its front hurts, low, jumpable. */
struct FWRForestBossWave
{
	FVector At = FVector::ZeroVector;
	double Start = 0.0;
	float MaxR = 800.f;
	float Damage = 40.f;
	float Speed = 1200.f;
	bool bLow = true;
	TSet<TWeakObjectPtr<AActor>> Done;
};

/** A survivor (or tame) held: in Old Gnarl's fist (grasp), lifted by Sylvaroth's root hand. */
struct FWRForestBossHold
{
	TWeakObjectPtr<AWRCharacterBase> Who;
	FName How;
	double Start = 0.0, Ends = 0.0, TickAt = 0.0;
	float Dps = 0.f;
	/** An ally's blows on the arm that holds (Old Gnarl's grasp: 100 frees). */
	float ArmDamage = 0.f;
	TWeakObjectPtr<AWRForestProp> Hand;
};

/** Server: one forest boss's fight. */
struct FWRForestBossState
{
	FName Kind;
	bool bBegun = false;
	/** Each row's own punish (the Hornwarden's enrage and the Matron's limp scale it). */
	TMap<FName, float> BasePunish;
	TArray<FWRForestBossWave> Waves;
	FWRForestBossHold Hold;
	/** Lobbed things still in the air (combat.md 2.17.1: one volley in phase 1, two in phase 2). */
	int32 PodsInFlight = 0;
	int32 StumpsInFlight = 0;
	/** Its arena's middle (ground) and radius. */
	FVector Home = FVector::ZeroVector;
	float ArenaR = 6000.f;

	// Old Gnarl: four bark plates (front, back, left arm, right arm).
	float PlateHP[4] = { 0.f, 0.f, 0.f, 0.f };
	double StrippedAt[4] = { 0.0, 0.0, 0.0, 0.0 };
	double BurnUntil[4] = { 0.0, 0.0, 0.0, 0.0 };
	double NoRegrowUntil[4] = { 0.0, 0.0, 0.0, 0.0 };
	double NextBurnStagger = 0.0;
	int32 RegrowPlate = -1;

	// The Bramblequeen.
	TArray<TWeakObjectPtr<AWRForestProp>> Combs;
	TArray<TWeakObjectPtr<AWRCreature>> Wasps;
	double NextSwarmAt = 0.0, StuckUntil = 0.0, SmokeUntil = 0.0, HiveHoldReady = 0.0;
	int32 SwarmComb = -1;
	FVector HiveFoot = FVector::ZeroVector;
	bool bHiveFoot = false;

	// The Grey Matron.
	TArray<FVector> Oaks;
	TArray<TWeakObjectPtr<AWRForestWalkway>> Walks;
	int32 Tree = 0, Walk = -1, GoalTree = -1, LeapTo = -1, ClimbTree = -1;
	float WalkT = 0.f;
	bool bForward = true;
	bool bCanopy = false, bGroundForGood = false, bClimbing = false, bToBell = false, bKits60 = false, bKits30 = false;
	FVector ClimbFrom = FVector::ZeroVector, ClimbBase = FVector::ZeroVector, ClimbTo = FVector::ZeroVector;
	FVector LeapFrom = FVector::ZeroVector, LeapEnd = FVector::ZeroVector;
	double ClimbStart = 0.0, ClimbEnd = 0.0, NextPatrol = 0.0, LastArrowSeen = 0.0, LandedAt = -100.0;
	TArray<TWeakObjectPtr<AWRCreature>> Kits;

	// The Hornwarden.
	TArray<TWeakObjectPtr<AWRCreature>> Herd;
	bool bHerdMade = false, bHuntHerd = false, bHunting = false, bWasHeld = false;
	double EnragedUntil = 0.0, NextBellowAt = 0.0, RestUntil = 0.0, RestedAt = -10000.0;
	int32 Route = -1;
	FName RouteKind;
	TArray<double> Holds;
	float AntlerHP[2] = { 0.f, 0.f };

	// Sylvaroth.
	TArray<TWeakObjectPtr<AWRForestRootLine>> Lines;
	TArray<TWeakObjectPtr<AWRForestProp>> Hearts;
	TArray<FVector> HeartAt;
	double NextSectorsAt = 0.0, NextShakeAt = 0.0;
	double NextHeartSweep[4] = { 0.0, 0.0, 0.0, 0.0 };
	double HeartHitAt[4] = { 0.0, 0.0, 0.0, 0.0 };
	bool bRooted = false, bRerooting = false;
	float SinkNow = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> SectorStand;
	TWeakObjectPtr<AWRCharacterBase> WhipAt;
	FVector EruptFrom = FVector::ZeroVector, EruptDir = FVector::ForwardVector;
	TWeakObjectPtr<AWRForestProp> HandProp;
};

namespace WRForestBossImpl
{
	TMap<TWeakObjectPtr<const AWRBoss>, FWRForestBossState>& FbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRForestBossState> Map;
		return Map;
	}

	FWRForestBossState& FbS(const AWRBoss* Boss) { return FbStates().FindOrAdd(Boss); }
	FWRForestBossState* FbFind(const AWRBoss* Boss) { return FbStates().Find(Boss); }

	double FbNow(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }

	/** The game state's server clock (what replicated "until" times are kept in, so every machine reads them alike). */
	float FbServerNow(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}

	double FbGameHours(const UObject* W)
	{
		const AWRGameState* GS = AWRGameState::Get(W);
		return GS ? GS->GetTotalHours() : 0.0;
	}

	float FbDamageScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	/** bosses.<id>.<key> (AWRBoss::Tune's own rule, for the helpers here that aren't the boss's friends). */
	float FbBossTune(const AWRBoss* Boss, const TCHAR* Key, float Fallback)
	{
		return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback);
	}

	FName FbKindOf(const AWRBoss* Boss)
	{
		if (!Boss) return NAME_None;
		const FName Id = Boss->CreatureId;
		return Id == TEXT("old_gnarl") || Id == TEXT("bramblequeen") || Id == TEXT("grey_matron") || Id == TEXT("hornwarden") || Id == TEXT("sylvaroth") ? Id : NAME_None;
	}

	FVector FbFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	/** The ground under a point (the land and static world only, never a body). */
	bool FbGround(const UWorld* World, const FVector& At, FVector& Out, float Up = 3000.f, float Down = 8000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestBossGround), false);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}

	/** Every survivor, tame and plant it may fight within Range of From (across the ground). */
	TArray<AWRCharacterBase*> FbFoes(const AWRBoss* Boss, float Range, const FVector& From)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || It->GetAttachParentActor()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), From) > Range + It->GetSimpleCollisionRadius()) continue;
			Out.Add(*It);
		}
		return Out;
	}

	float FbSegDist2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D AB(B - A), AP(P - A);
		const float T = FMath::Clamp(FVector2D::DotProduct(AP, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
		return FVector2D::Distance(FVector2D(A) + AB * T, FVector2D(P));
	}

	/** An axe in the hand: a hatchet (any tier) or a weapon whose id says axe. Axes strip bark and cut knots faster. */
	bool FbIsAxe(const AActor* By)
	{
		const AWRCharacter* C = Cast<AWRCharacter>(By);
		const FName HeldItem = C ? C->GetMeleeItem() : NAME_None;
		if (HeldItem.IsNone()) return false;
		const FWRToolDef* Tool = FWRGathering::FindTool(HeldItem);
		return (Tool && Tool->Kind == TEXT("hatchet")) || HeldItem.ToString().Contains(TEXT("axe"));
	}

	/** A fire blow: the element, a burn buildup, or a swing with a lit torch in the off hand (a torch on pale wood). */
	bool FbIsFire(const FWRHit& Hit)
	{
		return Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f || Hit.Buildups.FindRef(TEXT("burn")) > 0.f
			|| (Hit.bMelee && UWRSurvivalComponent::HoldsTorch(Hit.Instigator.Get()));
	}

	/** Where a blow came from on the body: the impact point, else where it was struck from, else the striker. */
	FVector FbHitFrom(const AWRBoss* Boss, const FWRHit& Hit)
	{
		if (!Hit.ImpactPoint.IsZero()) return Hit.ImpactPoint;
		if (!Hit.Origin.IsZero()) return Hit.Origin;
		return Hit.Instigator ? Hit.Instigator->GetActorLocation() : Boss->GetActorLocation() + Boss->GetActorForwardVector() * 100.f;
	}

	/** A blow that isn't its row's own (a wave, a slam after a hold, a heart-tree's sweep), with the boss's scaling. */
	void FbHitWith(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, bool bUnblockable, bool bLow, const FVector& Origin, FName Buildup = NAME_None, float Amount = 0.f)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * FbDamageScale() * (Boss->Phase >= 3 ? FbBossTune(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = Poise;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bHeavy = true;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = Origin;
		Hit.Element = WRTags::Element_Physical;
		Hit.PhysType = 2;
		if (!Buildup.IsNone() && Amount > 0.f) Hit.Buildups.Add(Buildup, Amount);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}

	/** A held body's squeeze: damage over time (no flinch), through any guard. */
	void FbSqueeze(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage)
	{
		if (!Victim || !Victim->IsAlive() || Damage <= 0.f) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * FbDamageScale();
		Hit.bUnblockable = true;
		Hit.bDamageOverTime = true;
		Hit.Origin = Boss->GetActorLocation();
		Hit.Element = WRTags::Element_Physical;
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}

	float FbHealthFraction(const AWRBoss* Boss)
	{
		const UWRAttributeSet* A = Boss->GetAttributes();
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}

	/** The plate a blow from From lands on: the front (within 50 deg of its facing), the back, else the arm on that side. */
	int32 FbPlateFor(const AWRBoss* Boss, const FVector& From)
	{
		const FVector To = (From - Boss->GetActorLocation()).GetSafeNormal2D();
		const float B = FMath::FindDeltaAngleDegrees(Boss->GetActorRotation().Yaw, To.Rotation().Yaw);
		if (FMath::Abs(B) <= 50.f) return 0;
		if (FMath::Abs(B) >= 130.f) return 1;
		return B < 0.f ? 2 : 3;
	}

	/** Where a plate sits on its body (body units: the server has no mesh). */
	FVector FbPlatePoint(const AWRBoss* Boss, int32 Plate)
	{
		const float R = Boss->GetSimpleCollisionRadius(), H = Boss->GetSimpleCollisionHalfHeight();
		const FVector F = Boss->GetActorForwardVector(), Rt = Boss->GetActorRightVector();
		switch (Plate)
		{
		case 0: return Boss->GetActorLocation() + F * R * 0.8f + FVector(0.f, 0.f, H * 0.35f);
		case 1: return Boss->GetActorLocation() - F * R * 0.8f + FVector(0.f, 0.f, H * 0.45f);
		case 2: return Boss->GetActorLocation() - Rt * R * 1.1f + FVector(0.f, 0.f, H * 0.3f);
		default: return Boss->GetActorLocation() + Rt * R * 1.1f + FVector(0.f, 0.f, H * 0.3f);
		}
	}

	bool FbPlateCanRegrow(const AWRBoss* Boss, const FWRForestBossState& St, int32 P, double T)
	{
		return St.PlateHP[P] <= 0.f && St.BurnUntil[P] <= T && St.NoRegrowUntil[P] <= T && T - St.StrippedAt[P] >= FbBossTune(Boss, TEXT("regrowEvery"), 40.f);
	}

	/** The Matron's walkways: which are standing, and which touch an oak. */
	bool FbWalkUp(const FWRForestBossState& St, int32 W) { return St.Walks.IsValidIndex(W) && St.Walks[W].IsValid() && !St.Walks[W]->bDown; }

	bool FbTreeHasWalk(const FWRForestBossState& St, int32 TreeIndex)
	{
		for (int32 W = 0; W < St.Walks.Num(); ++W)
		{
			if (FbWalkUp(St, W) && (St.Walks[W]->TreeA == TreeIndex || St.Walks[W]->TreeB == TreeIndex)) return true;
		}
		return false;
	}

	int32 FbWalksDown(const FWRForestBossState& St)
	{
		int32 N = 0;
		for (int32 W = 0; W < St.Walks.Num(); ++W) N += FbWalkUp(St, W) ? 0 : 1;
		return N;
	}

	/** Where she perches at an oak: the deck end of a standing walkway there, a step along it (her feet). */
	bool FbPerch(const FWRForestBossState& St, int32 TreeIndex, FVector& Out, int32* OutWalk = nullptr)
	{
		for (int32 W = 0; W < St.Walks.Num(); ++W)
		{
			if (!FbWalkUp(St, W)) continue;
			const AWRForestWalkway* Wk = St.Walks[W].Get();
			if (Wk->TreeA == TreeIndex) { Out = Wk->DeckAt(0.06f); if (OutWalk) *OutWalk = W; return true; }
			if (Wk->TreeB == TreeIndex) { Out = Wk->DeckAt(0.94f); if (OutWalk) *OutWalk = W; return true; }
		}
		return false;
	}

	/** The first walkway on the way from one oak to another over the standing walkways (a ring of six: breadth first). */
	int32 FbNextWalk(const FWRForestBossState& St, int32 From, int32 To)
	{
		if (From == To) return INDEX_NONE;
		TArray<int32> Prev, Via;
		Prev.Init(-2, St.Oaks.Num());
		Via.Init(INDEX_NONE, St.Oaks.Num());
		if (!Prev.IsValidIndex(From) || !Prev.IsValidIndex(To)) return INDEX_NONE;
		TArray<int32> Queue = { From };
		Prev[From] = -1;
		for (int32 q = 0; q < Queue.Num(); ++q)
		{
			const int32 N = Queue[q];
			for (int32 W = 0; W < St.Walks.Num(); ++W)
			{
				if (!FbWalkUp(St, W)) continue;
				const AWRForestWalkway* Wk = St.Walks[W].Get();
				const int32 Other = Wk->TreeA == N ? Wk->TreeB : Wk->TreeB == N ? Wk->TreeA : -1;
				if (!Prev.IsValidIndex(Other) || Prev[Other] != -2) continue;
				Prev[Other] = N;
				Via[Other] = W;
				Queue.Add(Other);
			}
		}
		if (Prev[To] == -2) return INDEX_NONE;
		int32 Step = To;
		while (Prev[Step] != From && Prev[Step] >= 0) Step = Prev[Step];
		return Via[Step];
	}

	/** The oak with a standing walkway nearest a point (her way down to you, her way back up), or -1. */
	int32 FbOakNear(const FWRForestBossState& St, const FVector& P, int32 Not = -1)
	{
		int32 Best = -1;
		float BestD = TNumericLimits<float>::Max();
		for (int32 T = 0; T < St.Oaks.Num(); ++T)
		{
			const float D = FVector::Dist2D(St.Oaks[T], P);
			if (T != Not && FbTreeHasWalk(St, T) && D < BestD) { BestD = D; Best = T; }
		}
		return Best;
	}

	/** A heart-tree's trunk at breast height (the tree kit's own proportions at the clearing's height). */
	float FbHeartRadius()
	{
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("heart")));
		return WRTreeKit::ReferenceRadius(TEXT("heart")) * WRTune(TEXT("bosses.sylvaroth.heartHeight"), 14000.f) / Ref;
	}

	float FbOakRadius(const AWRBoss* Boss)
	{
		const float OakH = WRTune(TEXT("bosses.grey_matron.oakHeight"), 7000.f);
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("bower_oak")));
		return WRTreeKit::ReferenceRadius(TEXT("bower_oak")) * OakH / Ref + (Boss ? Boss->GetSimpleCollisionRadius() : 0.f);
	}

	/** The Hornwarden's route: forest.json routes of this kind (roam, hunt), else his creatures.json loop. */
	TArray<FVector> FbRouteOf(const AWRBoss* Boss, FName Kind)
	{
		const FWRForestData& D = FWRForestData::Get();
		for (const FWRForestData::FPath& P : D.Routes)
		{
			if (P.Kind == Kind && P.Points.Num() >= 3) return P.Points;
		}
		if (Kind == TEXT("hunt"))
		{
			for (const FWRForestData::FPath& P : D.Routes) if (P.Kind == TEXT("roam") && P.Points.Num() >= 3) return P.Points;
		}
		const FWRCreatureDef* Def = Boss->GetDef();
		return Def ? Def->Roam : TArray<FVector>();
	}

	/** A site's ground point by kind or id (the Antler Barrow, the Hanging Oak, Herne's Ford). */
	bool FbSiteOfKind(FName Kind, FVector& Out)
	{
		for (const FWRForestData::FSite& S : FWRForestData::Get().Sites)
		{
			if (S.Kind == Kind || S.Id == Kind) { Out = S.At; return true; }
		}
		return false;
	}

	bool FbInSector(const UWRForestBossBody* Body, const FVector& P, int32& Out)
	{
		const FVector D = P - FVector(Body->SectorCentre);
		if (D.Size2D() > Body->SectorRadius) return false;
		float A = FMath::Atan2(D.Y, D.X);
		if (A < 0.f) A += 2.f * PI;
		Out = FMath::Clamp(int32(A / (2.f * PI / 6.f)), 0, 5);
		return true;
	}

	/** The furthest tame in its fight (the tame-punish rule), or null. */
	AWRCreature* FbFurthestTame(const AWRBoss* Boss)
	{
		AWRCreature* Best = nullptr;
		float BestD = -1.f;
		for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsTamed() || !It->IsAlive() || !Boss->IsHostileTo(*It) || FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) > Boss->GetChaseRadius()) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation());
			if (D > BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** The Nature path's plant summons in its fight (Sprout turret, World tree, Ancient guardian): the furthest. */
	AWRCharacterBase* FbFurthestPlant(const AWRBoss* Boss)
	{
		AWRCharacterBase* Best = nullptr;
		float BestD = -1.f;
		for (TActorIterator<AWRPlantSummon> It(Boss->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Boss->IsHostileTo(*It) || FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) > Boss->GetChaseRadius()) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation());
			if (D > BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** A survivor or a survivor's own (a tame, a plant): the side whose blows free a friend or hurt the herd. */
	bool FbSurvivorSide(const AActor* A)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(A);
		return C && (C->GetPlayerState() || !C->GetTameOwner().IsEmpty());
	}

	/** Server: a creature from a forest row at a point (null if the row isn't in creatures.json). */
	AWRCreature* FbSpawnRow(UWorld* World, FName Id, const FVector& At, const FRotator& Rot)
	{
		if (!FWRCreatureRegistry::Get().Find(Id))
		{
			UE_LOG(LogWildreach, Warning, TEXT("FOREST boss: creature %s has no row yet"), *Id.ToString());
			return nullptr;
		}
		return AWRCreature::SpawnCreature(World, Id, At, Rot);
	}

	/** Sylvaroth's moves from its first phase (timings x0.9 and damage x1.1 once it walks). */
	bool FbIsRootedMove(FName Id)
	{
		return Id == TEXT("sweep_low") || Id == TEXT("sweep_back") || Id == TEXT("sweep_slam") || Id == TEXT("root_eruption")
			|| Id == TEXT("seed_bombs") || Id == TEXT("root_hand") || Id == TEXT("thorn_whip");
	}

	/** Moves this file runs itself from the strike on (the attack machine only times them). */
	bool FbOwnStrike(FName Id)
	{
		static const TCHAR* const Own[] = {
			TEXT("boulder"), TEXT("regrow_bark"),
			TEXT("sting_lance"), TEXT("pheromone_mark"), TEXT("swarm_call"),
			TEXT("canopy_drop"), TEXT("bridge_leap"), TEXT("kit_scream"),
			TEXT("bellow"),
			TEXT("root_eruption"), TEXT("seed_bombs"), TEXT("root_hand"), TEXT("canopy_shake"), TEXT("thorn_whip"), TEXT("thrash_spin"), TEXT("re_root") };
		for (const TCHAR* Name : Own)
		{
			if (Id == FName(Name)) return true;
		}
		return false;
	}

	/** Sylvaroth's body into the ground (phase 1, the re-root) or out of it: the server moves the body; every machine
	 *  sees the lower body sunk and the torso rising out of the moss. */
	void FbSetRooted(AWRBoss* Boss, FWRForestBossState& St, bool bOn, float SinkBy)
	{
		UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
		const float Want = bOn ? SinkBy : 0.f;
		const float Delta = Want - St.SinkNow;
		if (!FMath::IsNearlyZero(Delta))
		{
			Boss->SetActorLocation(Boss->GetActorLocation() - FVector(0.f, 0.f, Delta), false, nullptr, ETeleportType::TeleportPhysics);
			St.SinkNow = Want;
		}
		if (bOn)
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Flying);
			Move->MaxFlySpeed = 0.f;
		}
		else if (Move->MovementMode != MOVE_Walking)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		St.bRooted = bOn;
		if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss))
		{
			Body->bRooted = bOn;
			Body->Sink = Want;
		}
	}
}

// ---------------------------------------------------------------- the contract (WRForestTypes.h)

void WRForest::StartWildHunt(UWorld* World, bool bStart)
{
	FWRForestBoss::SetWildHunt(World, bStart);
}

bool WRForest::WildHuntActive(const UObject* WorldContext)
{
	const AWRForestArenaState* State = AWRForestArenaState::Get(WorldContext);
	return State && State->bWildHunt;
}

void WRForest::SummonMatronToBell(UWorld* World, const FVector& Bell)
{
	// Only at night (F-S5): by day the bell rings for the finches (WRForestHazards' own answer).
	if (World && AWRGameState::IsNightIn(World))
	{
		FWRForestBoss::MatronToBell(World, Bell);
	}
}

void FWRForestBoss::SetWildHunt(UWorld* World, bool bOn)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	UWRForestArenas* Arenas = UWRForestArenas::Get(World);
	if (Arenas && Arenas->bWildHunt == bOn)
	{
		return;
	}
	if (Arenas) Arenas->bWildHunt = bOn;
	if (AWRForestArenaState* State = AWRForestArenaState::Get(World))
	{
		State->bWildHunt = bOn;
		State->ForceNetUpdate();
	}
	if (bOn)
	{
		// The horn over the whole forest: everyone in it hears the Hunt ride.
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (It->IsAlive() && WRForest::Contains(It->GetActorLocation())) WRSound::PlayOn(*It, TEXT("forest.hunt_horn"), 0.8f);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST the Wild Hunt %s"), bOn ? TEXT("rides") : TEXT("is over"));
}

void FWRForestBoss::MatronToBell(UWorld* World, const FVector& Bell)
{
	using namespace WRForestBossImpl;
	UWRForestArenas* Arenas = UWRForestArenas::Get(World);
	if (!Arenas || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	Arenas->BellAt = Bell;
	Arenas->BellUntil = World->GetTimeSeconds() + WRTune(TEXT("bosses.grey_matron.bellHuntSeconds"), 600.f);
	for (TActorIterator<AWRBoss> It(World); It; ++It)
	{
		if (It->CreatureId != TEXT("grey_matron") || !It->IsAlive() || It->bEngaged) continue;
		FWRForestBossState& St = FbS(*It);
		if (St.bToBell) continue;
		// She leaves the Bower and hunts the bell: down from the canopy, slow on the ground, no walkways out there.
		St.bToBell = true;
		St.bClimbing = false;
		if (St.bCanopy) MatronDrop(*It, St, false);
		It->SetHome(Bell);
		It->bReturning = true;
		It->ReturnStarted = It->GetWorld()->GetTimeSeconds();
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Grey Matron leaves the Bower for the bell"));
	}
}

// ---------------------------------------------------------------- the bosses: life

bool FWRForestBoss::IsForestBoss(const AWRBoss* Boss)
{
	return !WRForestBossImpl::FbKindOf(Boss).IsNone();
}

bool FWRForestBoss::IsHerd(const AActor* Who)
{
	for (const TPair<TWeakObjectPtr<const AWRBoss>, FWRForestBossState>& P : WRForestBossImpl::FbStates())
	{
		if (P.Value.Kind != TEXT("hornwarden")) continue;
		for (const TWeakObjectPtr<AWRCreature>& H : P.Value.Herd) if (H.Get() == Who) return true;
	}
	return false;
}

void FWRForestBoss::Begin(AWRBoss* Boss)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || !Boss->HasAuthority())
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	St.Kind = FbKindOf(Boss);
	St.bBegun = true;
	UWRForestBossBody::Attach(Boss);
	for (const FWRBossAttack& A : Boss->Attacks)
	{
		if (A.bRow) St.BasePunish.Add(A.Id, A.Row.Punish);
	}
	TArray<FVector> Points;
	if (!Arena(St.Kind, St.Home, St.ArenaR, Points))
	{
		St.Home = Boss->GetHome();
		St.ArenaR = Boss->ArenaRadius;
	}
	// A body this size steps over what a survivor climbs (the Clear-cut's stumps, the clearing's roots).
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->MaxStepHeight = FMath::Clamp(Boss->Tune(TEXT("stepHeight"), Move->MaxStepHeight), Move->MaxStepHeight, Boss->GetSimpleCollisionHalfHeight() * 0.5f);
	}
	if (St.Kind == TEXT("grey_matron"))
	{
		TArray<FVector> A, B;
		TArray<FIntPoint> Trees;
		BowerLayout(St.Oaks, A, B, Trees);
		for (FVector& O : St.Oaks) FbGround(Boss->GetWorld(), O, O, 20000.f, 40000.f);
	}
	if (St.Kind == TEXT("sylvaroth"))
	{
		FVector Centre;
		TArray<FVector> Stones;
		HeartsLayout(Centre, St.HeartAt, Stones);
	}
	if (St.Kind == TEXT("bramblequeen"))
	{
		FVector Base;
		TArray<float> Floors;
		TArray<FVector> Combs;
		if (HiveLayout(Base, Floors, Combs)) St.bHiveFoot = FbGround(Boss->GetWorld(), Base, St.HiveFoot, 20000.f, 40000.f);
	}
	Reset(Boss);
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s (forest) begins"), *Boss->GetDisplayLabel());
}

void FWRForestBoss::Reset(AWRBoss* Boss)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || !Boss->HasAuthority())
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	ReleaseGrab(Boss, St, false);
	St.Waves.Reset();
	St.PodsInFlight = St.StumpsInFlight = 0;
	Boss->SpeedBoost = 1.f;
	if (Body) Body->Pose = 0;
	const FName K = St.Kind;
	const double T = FbNow(Boss);
	if (K == TEXT("old_gnarl"))
	{
		for (int32 P = 0; P < 4; ++P)
		{
			St.PlateHP[P] = Boss->Tune(TEXT("barkPlateHealth"), 400.f);
			St.StrippedAt[P] = St.BurnUntil[P] = St.NoRegrowUntil[P] = 0.0;
		}
		St.RegrowPlate = -1;
		St.NextBurnStagger = 0.0;
		if (Body) { Body->Stripped = 0; Body->Burning = 0; }
	}
	else if (K == TEXT("bramblequeen"))
	{
		St.Combs.Reset();
		for (TActorIterator<AWRForestProp> It(Boss->GetWorld()); It; ++It)
		{
			if (It->Kind == TEXT("comb") && FVector::Dist2D(It->GetActorLocation(), St.Home) < St.ArenaR + 2000.f)
			{
				// Her brood grows back with her (a fresh fight).
				It->Health = It->MaxHealth = Boss->Tune(TEXT("combHealth"), 300.f);
				It->SetState(0);
				St.Combs.Add(*It);
			}
		}
		St.Combs.Sort([](const TWeakObjectPtr<AWRForestProp>& A, const TWeakObjectPtr<AWRForestProp>& B) { return A->Index < B->Index; });
		for (const TWeakObjectPtr<AWRCreature>& W : St.Wasps) if (W.IsValid() && W->IsAlive()) W->Destroy();
		St.Wasps.Reset();
		St.NextSwarmAt = T + Boss->Tune(TEXT("firstSwarm"), 8.f);
		St.StuckUntil = St.SmokeUntil = St.HiveHoldReady = 0.0;
		// She flies the hive's open middle, between its galleries.
		UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
		Move->SetMovementMode(MOVE_Flying);
		Move->MaxFlySpeed = Boss->GetDef() ? Boss->GetDef()->Speed : 600.f;
		if (Body) Body->bStuck = false;
	}
	else if (K == TEXT("grey_matron"))
	{
		St.Walks.Reset();
		for (TActorIterator<AWRForestWalkway> It(Boss->GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), St.Home) < St.ArenaR + 3000.f) St.Walks.Add(*It);
		}
		St.Walks.Sort([](const TWeakObjectPtr<AWRForestWalkway>& A, const TWeakObjectPtr<AWRForestWalkway>& B) { return A->TreeA < B->TreeA; });
		for (const TWeakObjectPtr<AWRCreature>& Kit : St.Kits) if (Kit.IsValid() && Kit->IsAlive()) Kit->SetHome(Kit->GetActorLocation());
		St.Kits.Reset();
		St.bKits60 = St.bKits30 = false;
		St.Walk = St.GoalTree = St.LeapTo = -1;
		St.bClimbing = false;
		if (St.bToBell && UWRForestArenas::Get(Boss) && FbNow(Boss) >= UWRForestArenas::Get(Boss)->BellUntil)
		{
			// The bell's hunt is over: she goes home to the Bower.
			St.bToBell = false;
			Boss->SetHome(St.Home);
		}
		// Up into the canopy if her walkways stand (4 or more down: she stays down until they grow back). If she's
		// already in it she stays where she is; on the ground she climbs back once home (TickIdle).
		St.bGroundForGood = FbWalksDown(St) >= FMath::RoundToInt(Boss->Tune(TEXT("groundAt"), 4.f));
		if (St.bGroundForGood && St.bCanopy) MatronDrop(Boss, St, true);
		if (Body) Body->bGrounded = St.bGroundForGood;
	}
	else if (K == TEXT("hornwarden"))
	{
		St.EnragedUntil = 0.0;
		St.NextBellowAt = 0.0;
		St.Holds.Reset();
		St.bWasHeld = false;
		St.AntlerHP[0] = St.AntlerHP[1] = Boss->Tune(TEXT("antlerPartHealth"), 1200.f);
		// The herd that charged on his bellow walks with him again.
		for (const TWeakObjectPtr<AWRCreature>& H : St.Herd) if (H.IsValid()) { H->SetAggroTarget(nullptr); H->SpeedBoost = 1.f; }
		if (Body) Body->bEnraged = St.bHunting;
	}
	else if (K == TEXT("sylvaroth"))
	{
		St.Lines.Reset();
		for (TActorIterator<AWRForestRootLine> It(Boss->GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(FVector(It->To), St.Home) < 3000.f)
			{
				It->KnotHealth = Boss->Tune(TEXT("knotHealth"), 900.f);
				It->bHeartOnly = false;
				It->DarkUntil = 0.f;
				It->Why = 0;
				It->ForceNetUpdate();
				St.Lines.Add(*It);
			}
		}
		St.Lines.Sort([](const TWeakObjectPtr<AWRForestRootLine>& A, const TWeakObjectPtr<AWRForestRootLine>& B) { return A->Index < B->Index; });
		St.Hearts.Reset();
		for (TActorIterator<AWRForestProp> It(Boss->GetWorld()); It; ++It)
		{
			if (It->Kind == TEXT("heart_tree") && FVector::Dist2D(It->GetActorLocation(), St.Home) < St.ArenaR + 2000.f)
			{
				It->Health = It->MaxHealth = Boss->Tune(TEXT("p3HeartHealth"), 1500.f);
				It->SetState(0);
				St.Hearts.Add(*It);
			}
		}
		St.Hearts.Sort([](const TWeakObjectPtr<AWRForestProp>& A, const TWeakObjectPtr<AWRForestProp>& B) { return A->Index < B->Index; });
		if (AWRForestProp* Hand = St.HandProp.Get()) Hand->Destroy();
		St.HandProp = nullptr;
		St.bRerooting = false;
		St.SectorStand.Reset();
		St.NextSectorsAt = St.NextShakeAt = 0.0;
		for (int32 i = 0; i < 4; ++i) St.NextHeartSweep[i] = St.HeartHitAt[i] = 0.0;
		if (Body)
		{
			Body->Sectors.Init(0, 6);
			Body->SectorUntil.Init(0.f, 6);
			Body->SectorCentre = St.Home;
			Body->SectorRadius = St.ArenaR;
		}
		// Rooted in the clearing once it stands home on the ground (TickIdle roots it there).
	}
}

void FWRForestBoss::EndPlay(AWRBoss* Boss)
{
	using namespace WRForestBossImpl;
	if (FWRForestBossState* St = FbFind(Boss))
	{
		ReleaseGrab(Boss, *St, false);
		for (const TWeakObjectPtr<AWRCreature>& W : St->Wasps) if (W.IsValid() && W->IsAlive()) W->SetHome(W->GetActorLocation());
		if (AWRForestProp* Hand = St->HandProp.Get()) Hand->Destroy();
	}
	FbStates().Remove(Boss);
}

void FWRForestBoss::Died(AWRBoss* Boss)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	ReleaseGrab(Boss, St, false);
	UWorld* World = Boss->GetWorld();
	UWRForestArenas* Arenas = UWRForestArenas::Get(World);
	const FName K = St.Kind;
	// Everyone who earned its loot (the 10% rule), for the extras below.
	TArray<AWRPlayerState*> Earned;
	const UWRAttributeSet* Attr = Boss->GetAttributes();
	const float Needed = (Attr ? Attr->GetMaxHealth() : 1000.f) * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	for (const TPair<FString, float>& E : Boss->Ledger)
	{
		if (E.Value < Needed || !World->GetGameState()) continue;
		for (APlayerState* P : World->GetGameState()->PlayerArray)
		{
			if (AWRPlayerState* PS = Cast<AWRPlayerState>(P); PS && PS->GetPlayerName() == E.Key) Earned.Add(PS);
		}
	}
	if (K == TEXT("hornwarden"))
	{
		// The herd scatters (wild Elderelk again); killed in the Wild Hunt: a second trophy roll and the Huntsman's crown.
		for (const TWeakObjectPtr<AWRCreature>& H : St.Herd)
		{
			if (!H.IsValid()) continue;
			H->SetHome(H->GetActorLocation());
			H->SetAggroTarget(nullptr);
			H->SpeedBoost = 1.f;
		}
		St.Herd.Reset();
		if (St.bHunting)
		{
			for (AWRPlayerState* PS : Earned)
			{
				if (!PS->GetInventory()) continue;
				FString Got;
				if (FMath::FRand() < Boss->Tune(TEXT("huntTrophyChance"), 1.f))
				{
					PS->GetInventory()->AddItem(TEXT("trophy_warden_antler"), 1);
					Got += TEXT("Warden antler  ");
				}
				PS->GetInventory()->AddItem(TEXT("huntsmans_crown"), 1);
				Got += TEXT("Huntsman's crown");
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("Killed in the Wild Hunt: %s"), *Got));
			}
			SetWildHunt(World, false);
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden falls in the Wild Hunt"));
		}
	}
	else if (K == TEXT("grey_matron"))
	{
		// Her walkways regrow 4 game hours after she dies (living Grovekin work); the larder's hide curtain falls (F-S6).
		if (Arenas) Arenas->MatronDiedHours = FbGameHours(World);
		int32 Cut = 0;
		for (const TWeakObjectPtr<AWRForestWalkway>& W : St.Walks) Cut += W.IsValid() && W->bDown && W->bCutByPlayer ? 1 : 0;
		if (Cut >= FMath::RoundToInt(Boss->Tune(TEXT("questWalkways"), 4.f)))
		{
			if (UWRWorldSystems* WS = World->GetSubsystem<UWRWorldSystems>())
			{
				for (AWRPlayerState* PS : Earned) WS->QuestEvent(PS, TEXT("grey_matron_walkways"), NAME_None);
			}
		}
		for (TActorIterator<AWRForestProp> It(World); It; ++It)
		{
			if (It->Kind != TEXT("larder") || It->State != 0 || FVector::Dist2D(It->GetActorLocation(), St.Home) > St.ArenaR + 3000.f) continue;
			It->SetState(1);
			WRFx::Broadcast(Boss, TEXT("matron.curtain_fall"), It->GetActorLocation() + FVector(0.f, 0.f, 250.f), It->GetActorRotation(), 1.f, TEXT("forest.walkway_fall"));
			// What she kept in the hollow: hides x10, a Thornlynx kit, old Grovekin jewellery (anyone may take them).
			TArray<FWRItemStack> Items;
			const auto Put = [&Items](FName Id, int32 N)
			{
				if (!FWRItemRegistry::Get().Find(Id)) { UE_LOG(LogWildreach, Warning, TEXT("FOREST larder: item %s has no row yet"), *Id.ToString()); return; }
				FWRItemStack Stack;
				Stack.ItemId = Id;
				Stack.Count = N;
				Items.Add(Stack);
			};
			Put(TEXT("hide"), FMath::RoundToInt(Boss->Tune(TEXT("larderHides"), 10.f)));
			Put(TEXT("egg_thornlynx"), 1);   // the kit: hatched in an incubator it is a tame Thornlynx (F-S6)
			Put(TEXT("grovekin_jewellery"), 1);
			if (Items.Num() > 0)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FVector Inside = It->GetActorLocation() + It->GetActorForwardVector() * -120.f + FVector(0.f, 0.f, 40.f);
				if (AWRLootBag* Bag = World->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), Inside, FRotator::ZeroRotator, Params))
				{
					Bag->OwnerName.Reset();
					Bag->bOwnerOnly = false;
					Bag->Items = Items;
					Bag->SetLifeSpan(WRTune(TEXT("death.bagMinutes"), 30.f) * 60.f * 4.f);
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("FOREST the Matron's larder opens"));
		}
		for (const TWeakObjectPtr<AWRCreature>& Kit : St.Kits) if (Kit.IsValid() && Kit->IsAlive()) Kit->SetHome(Kit->GetActorLocation());
	}
	else if (K == TEXT("sylvaroth"))
	{
		// The Four Hearts' bark weeps resin for a game day (20 nodes on the four trunks).
		if (Arenas)
		{
			Arenas->ResinUntilHours = FbGameHours(World) + Boss->Tune(TEXT("resinHours"), 24.f);
			const int32 Per = FMath::Max(1, FMath::RoundToInt(Boss->Tune(TEXT("resinNodes"), 20.f) / FMath::Max(1, St.HeartAt.Num())));
			const float TrunkR = FbHeartRadius();
			for (int32 H = 0; H < St.HeartAt.Num(); ++H)
			{
				FVector Foot;
				FbGround(World, St.HeartAt[H], Foot, 20000.f, 40000.f);
				for (int32 i = 0; i < Per; ++i)
				{
					// Round the trunk's face toward the clearing (where the sap runs in the cracks), knee to head height.
					const FVector Out = (St.Home - Foot).GetSafeNormal2D().RotateAngleAxis(-70.f + 140.f * (i + 0.5f) / Per, FVector::UpVector);
					const FVector At = Foot + Out * (TrunkR + 25.f) + FVector(0.f, 0.f, 60.f + 140.f * float((i * 37) % 5) / 4.f);
					if (AWRResourceNode* Node = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), FTransform(Out.Rotation(), At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
					{
						Node->ItemId = TEXT("resin");
						Node->Hits = FMath::RoundToInt(Boss->Tune(TEXT("resinHits"), 3.f));
						Node->PerHit = FMath::RoundToInt(Boss->Tune(TEXT("resinPerHit"), 2.f));
						Node->RespawnSeconds = 1.0e7f;   // it weeps once; the day's end takes the nodes away
						Node->FinishSpawning(FTransform(Out.Rotation(), At));
						Arenas->ResinNodes.Add(Node);
					}
				}
			}
		}
		for (const TWeakObjectPtr<AWRForestRootLine>& L : St.Lines) if (L.IsValid()) L->Darken(1.0e6f, 1);
		for (const TWeakObjectPtr<AWRForestProp>& H : St.Hearts) if (H.IsValid()) H->SetState(0);
		if (AWRForestProp* Hand = St.HandProp.Get()) Hand->Destroy();
		WRFx::Broadcast(Boss, TEXT("sylvaroth.resin_weep"), St.Home, FRotator::ZeroRotator, 3.f, TEXT("forest.root_pulse"), 8.f);
	}
	else if (K == TEXT("bramblequeen"))
	{
		// Her swarm loses its queen: what's left of it scatters to the hive's paper (wild wasps again).
		for (const TWeakObjectPtr<AWRCreature>& W : St.Wasps) if (W.IsValid() && W->IsAlive()) { W->SetHome(W->GetActorLocation()); W->SetAggroTarget(nullptr); }
	}
	if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss))
	{
		Body->Pose = 0;
		Body->Burning = 0;
		Body->bStuck = false;
		Body->Sectors.Init(0, 6);
	}
}

// ---------------------------------------------------------------- not fighting

bool FWRForestBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	using namespace WRForestBossImpl;
	bSkipWake = false;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	FWRForestBossState& St = FbS(Boss);
	const double T = FbNow(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	if (St.Kind == TEXT("hornwarden"))
	{
		return TickWardenRoute(Boss, St, DeltaSeconds, false);
	}
	if (St.Kind == TEXT("sylvaroth"))
	{
		// Rooted in its clearing (its lower body in the earth) once it stands home on the ground; walking home first after
		// a reset that left it out in the clearing.
		if (!St.bRooted && !Boss->bReturning && FVector::Dist2D(Boss->GetActorLocation(), Boss->GetHome()) < 500.f && Move->IsMovingOnGround())
		{
			FbSetRooted(Boss, St, true, Boss->Tune(TEXT("rootSink"), 600.f));
			WRFx::Broadcast(Boss, TEXT("sylvaroth.reroot"), FbFeet(Boss), Boss->GetActorRotation(), Boss->GetSimpleCollisionRadius() / 200.f, TEXT("forest.root_pulse"));
		}
		return St.bRooted || !Boss->bReturning;
	}
	if (St.Kind == TEXT("bramblequeen"))
	{
		// She keeps to the hive's open middle, turning slowly over her brood.
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		if (!Boss->bReturning)
		{
			Boss->SetActorRotation(FRotator(0.f, Boss->GetActorRotation().Yaw + 12.f * DeltaSeconds, 0.f));
		}
		return true;
	}
	if (St.Kind == TEXT("grey_matron"))
	{
		UWRForestArenas* Arenas = UWRForestArenas::Get(Boss);
		if (St.bToBell)
		{
			// At the bell she prowls it; the hunt over, she walks home to the Bower.
			if (!Arenas || T >= Arenas->BellUntil)
			{
				St.bToBell = false;
				Boss->SetHome(St.Home);
				Boss->bReturning = true;
				Boss->ReturnStarted = T;
			}
			Boss->SpeedBoost = Boss->Tune(TEXT("groundSpeed"), 0.6f);
			return false;
		}
		if (St.bClimbing)
		{
			TickMatronClimb(Boss, St);
			return true;
		}
		if (!St.bCanopy)
		{
			if (Move->MovementMode == MOVE_Flying) Move->SetMovementMode(MOVE_Falling);   // reset mid-leap: down first
			// Home on the moss with her walkways standing: she climbs back up the nearest oak that has one.
			St.bGroundForGood = FbWalksDown(St) >= FMath::RoundToInt(Boss->Tune(TEXT("groundAt"), 4.f));
			if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->bGrounded = St.bGroundForGood;
			const int32 Oak = FbOakNear(St, Boss->GetActorLocation());
			if (!St.bGroundForGood && Oak >= 0 && !Boss->bReturning && !Move->IsFalling() && FVector::Dist2D(Boss->GetActorLocation(), St.Home) < St.ArenaR + 2000.f)
			{
				MatronClimb(Boss, St, Oak);
				return true;
			}
			Boss->SpeedBoost = Boss->Tune(TEXT("groundSpeed"), 0.6f);
			return false;
		}
		// In the canopy: now and then she walks the walkways to another oak (the shadow passing over the moss).
		if (St.Walk < 0 && T >= St.NextPatrol)
		{
			St.NextPatrol = T + FMath::FRandRange(Boss->Tune(TEXT("patrolMin"), 20.f), Boss->Tune(TEXT("patrolMax"), 45.f));
			St.GoalTree = FMath::RandRange(0, FMath::Max(0, St.Oaks.Num() - 1));
		}
		MatronCanopyMove(Boss, St, DeltaSeconds);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- fighting: before the attack machine

bool FWRForestBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	FWRForestBossState& St = FbS(Boss);
	TickWaves(Boss, St);
	TickHolds(Boss, St, DeltaSeconds);
	const FName K = St.Kind;
	if (K == TEXT("old_gnarl")) return TickGnarl(Boss, St, DeltaSeconds);
	if (K == TEXT("bramblequeen")) return TickQueen(Boss, St, DeltaSeconds);
	if (K == TEXT("grey_matron")) return TickMatron(Boss, St, DeltaSeconds);
	if (K == TEXT("hornwarden")) return TickWarden(Boss, St, DeltaSeconds);
	return TickSylvaroth(Boss, St, DeltaSeconds);
}

bool FWRForestBoss::Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target)
{
	if (!Boss || !Target || Boss->Current != INDEX_NONE)
	{
		return false;
	}
	const int32 i = Boss->Attacks.IndexOfByPredicate([Id](const FWRBossAttack& A) { return A.Id == Id; });
	if (!Boss->Attacks.IsValidIndex(i) || Boss->Attacks[i].MinPhase > Boss->Phase)
	{
		return false;
	}
	Boss->StartAttack(Boss->Attacks[i], Target);
	return true;
}

void FWRForestBoss::CancelAttack(AWRBoss* Boss)
{
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike)
	{
		EndStrike(Boss, Boss->Attacks[Boss->Current]);
	}
	Boss->Current = INDEX_NONE;
	Boss->StrikeState = AWRBoss::EStrike::None;
	Boss->SpeedBoost = 1.f;
	if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->Pose = 0;
}

bool FWRForestBoss::TickGnarl(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	// A burning spot keeps burning while the fire on the body lasts (fresh fire on it refreshes it).
	uint8 Burn = 0, Strip = 0;
	int32 NumBurning = 0;
	for (int32 P = 0; P < 4; ++P)
	{
		if (St.PlateHP[P] <= 0.f)
		{
			Strip |= uint8(1 << P);
			if (Boss->HasTag(WRTags::Status_Burn) && St.BurnUntil[P] > 0.0 && T < St.BurnUntil[P] + 0.5) St.BurnUntil[P] = FMath::Max(St.BurnUntil[P], T + 0.5);
			if (T < St.BurnUntil[P]) { Burn |= uint8(1 << P); ++NumBurning; }
		}
	}
	if (Body) { Body->Stripped = Strip; Body->Burning = Burn; }
	// Two spots burning at once: it reels, beating at the flames (3 s every 10 s while both burn).
	if (NumBurning >= 2 && T >= St.NextBurnStagger && !Boss->HasTag(WRTags::Status_Staggered))
	{
		St.NextBurnStagger = T + Boss->Tune(TEXT("burnStaggerEvery"), 10.f);
		CancelAttack(Boss);
		Boss->Status->Stagger(Boss->Tune(TEXT("burnStaggerSeconds"), 3.f));
		WRFx::Broadcast(Boss, TEXT("gnarl.reel"), Boss->GetActorLocation(), Boss->GetActorRotation(), Boss->GetSimpleCollisionRadius() / 150.f, TEXT("forest.bark_strip"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS Old Gnarl reels: two spots burning"));
		return true;
	}
	// A stripped spot that isn't burning (nor blighted) grows its bark back: the regrow move, one plate at a time.
	if (Boss->Current == INDEX_NONE && T >= Boss->BusyUntil && !Boss->HasTag(WRTags::Status_Staggered))
	{
		for (int32 P = 0; P < 4; ++P)
		{
			if (!FbPlateCanRegrow(Boss, St, P, T)) continue;
			AWRCharacterBase* Target = Boss->PickBossTarget();
			St.RegrowPlate = P;
			if (Target && Force(Boss, TEXT("regrow_bark"), Target)) return true;
			St.RegrowPlate = -1;
			break;
		}
	}
	return false;
}

bool FWRForestBoss::TickQueen(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
	St.Wasps.RemoveAll([](const TWeakObjectPtr<AWRCreature>& W) { return !W.IsValid() || !W->IsAlive(); });
	int32 Live = 0;
	for (const TWeakObjectPtr<AWRForestProp>& C : St.Combs) Live += C.IsValid() && C->State == 0 ? 1 : 0;
	// Smoke in the hive slows her (x0.7 for 5 s); with every comb broken she has nothing to guard (x0.8).
	if (WRForest::InSmoke(Boss, Boss->GetActorLocation())) St.SmokeUntil = T + Boss->Tune(TEXT("smokeSeconds"), 5.f);
	const FWRCreatureDef* Def = Boss->GetDef();
	float Speed = Def ? Def->Speed : 600.f;
	if (T < St.SmokeUntil) Speed *= Boss->Tune(TEXT("smokeSlow"), 0.7f);
	if (Live == 0 && St.Combs.Num() > 0) Speed *= Boss->Tune(TEXT("noCombsSpeed"), 0.8f);
	Move->MaxFlySpeed = Speed;
	// Three Hivebacks balled on her hold her 2 s (she's too big to cook).
	int32 Clinging = 0;
	for (const TWeakObjectPtr<UWRForestBody>& FB : UWRForestBody::Live)
	{
		const UWRForestBody* B = FB.Get();
		Clinging += B && B->Has(WRForestFlag::Clinging) && B->ClingTo.Get() == Boss ? 1 : 0;
	}
	if (Clinging >= FMath::RoundToInt(Boss->Tune(TEXT("hivebacksToHold"), 3.f)) && T >= St.HiveHoldReady)
	{
		St.HiveHoldReady = T + Boss->Tune(TEXT("hiveHoldEvery"), 8.f);
		CancelAttack(Boss);
		Boss->Status->Stagger(Boss->Tune(TEXT("hiveHoldSeconds"), 2.f));
		WRFx::BroadcastOn(Boss, TEXT("queen.hive_hold"), NAME_None, 2.f, TEXT("forest.heat_ball"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Bramblequeen held by %d Hivebacks"), Clinging);
		return true;
	}
	// The stinger stuck in the paper: she wrenches at it (1.1 s), open.
	const bool bStuck = T < St.StuckUntil;
	if (Body) Body->bStuck = bStuck;
	if (bStuck)
	{
		Move->StopMovementImmediately();
		return true;
	}
	// She keeps inside the hive's open middle (round the dead oak, under the dome) and rises or sinks to the height of
	// whoever she's after; the shell's paper stops her sideways (her flying moves sweep).
	if (St.bHiveFoot)
	{
		const float TrunkR = Boss->Tune(TEXT("trunkClear"), 450.f);
		const float Inner = WRTune(TEXT("bosses.bramblequeen.galleryOut"), 1300.f) - Boss->GetSimpleCollisionRadius();
		FVector P = Boss->GetActorLocation();
		FVector2D Off(P - St.HiveFoot);
		const float R = Off.Size();
		if (R < TrunkR) Off = (R > 1.f ? Off / R : FVector2D(1.f, 0.f)) * TrunkR;
		else if (R > Inner && FVector::Dist2D(P, St.HiveFoot) < Inner + 1500.f) Off = Off / R * Inner;
		AWRCharacterBase* Aim = Boss->AttackTarget.IsValid() ? Boss->AttackTarget.Get() : Boss->PickBossTarget();
		const float Low = St.HiveFoot.Z + Boss->GetSimpleCollisionHalfHeight() + 60.f;
		const float High = St.HiveFoot.Z + WRTune(TEXT("bosses.bramblequeen.hiveHeight"), 2100.f) - Boss->GetSimpleCollisionHalfHeight() - 80.f;
		float WantZ = Aim ? Aim->GetActorLocation().Z + Boss->Tune(TEXT("hoverOver"), 120.f) : P.Z;
		WantZ = FMath::Clamp(WantZ, Low, High);
		const float NewZ = Boss->Current == INDEX_NONE ? FMath::FInterpConstantTo(P.Z, WantZ, Dt, Speed * 0.6f) : P.Z;
		const FVector Want(St.HiveFoot.X + Off.X, St.HiveFoot.Y + Off.Y, FMath::Clamp(NewZ, Low, High));
		if (!Want.Equals(P, 1.f)) Boss->SetActorLocation(Want, true);
	}
	// The swarm: every 25 s while a comb of hers lives, she bows over it and calls six from it.
	if (Live > 0 && T >= St.NextSwarmAt && Boss->Current == INDEX_NONE && T >= Boss->BusyUntil)
	{
		AWRCharacterBase* Target = Boss->PickBossTarget();
		if (Target && Force(Boss, TEXT("swarm_call"), Target)) return true;
	}
	return false;
}

bool FWRForestBoss::TickMatron(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	St.Kits.RemoveAll([](const TWeakObjectPtr<AWRCreature>& Kit) { return !Kit.IsValid() || !Kit->IsAlive(); });
	if (St.bClimbing)
	{
		TickMatronClimb(Boss, St);
		return true;
	}
	// A drop or a leap cut short in the air (a stagger): she falls the rest of the way.
	if (!St.bCanopy && Boss->Current == INDEX_NONE && Boss->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		Boss->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		St.LandedAt = T;
	}
	// The walkways: 4 of 6 down, or the oak she's in left with none: she comes down for good.
	const int32 Down = FbWalksDown(St);
	if (!St.bGroundForGood && St.Walks.Num() > 0 && (Down >= FMath::RoundToInt(Boss->Tune(TEXT("groundAt"), 4.f)) || (St.bCanopy && St.Walk < 0 && !FbTreeHasWalk(St, St.Tree))))
	{
		CancelAttack(Boss);
		MatronDrop(Boss, St, true);
		return true;
	}
	// The one she's walking falls under her: she drops with it (and climbs back while she still can).
	if (St.bCanopy && St.Walk >= 0 && !FbWalkUp(St, St.Walk))
	{
		CancelAttack(Boss);
		MatronDrop(Boss, St, false);
		return true;
	}
	// Kit screams at 60% and 30%.
	const float Frac = FbHealthFraction(Boss);
	if (Boss->Current == INDEX_NONE && ((!St.bKits60 && Frac <= Boss->Tune(TEXT("kitsAt1"), 0.6f)) || (!St.bKits30 && Frac <= Boss->Tune(TEXT("kitsAt2"), 0.3f))))
	{
		AWRCharacterBase* Target = Boss->PickBossTarget();
		if (Target && Force(Boss, TEXT("kit_scream"), Target))
		{
			if (!St.bKits60) St.bKits60 = true; else St.bKits30 = true;
			return true;
		}
	}
	if (St.bCanopy)
	{
		if (Body) Body->bGrounded = false;
		// Arrows thudding into the walkway she's on make her leap to another (she hates the noise).
		int32 On = St.Walk;
		FVector Perch;
		if (On < 0) FbPerch(St, St.Tree, Perch, &On);
		if (St.Walks.IsValidIndex(On) && St.Walks[On].IsValid() && St.Walks[On]->ArrowAt > St.LastArrowSeen && T - St.Walks[On]->ArrowAt < 1.0)
		{
			St.LastArrowSeen = St.Walks[On]->ArrowAt;
			if (Boss->Current == INDEX_NONE)
			{
				AWRCharacterBase* Target = Boss->PickBossTarget();
				if (Target && Force(Boss, TEXT("bridge_leap"), Target)) return true;
			}
		}
		// Through the canopy toward the oak over whoever she's after; her drops come from there.
		if (Boss->Current == INDEX_NONE)
		{
			if (AWRCharacterBase* Target = Boss->PickBossTarget())
			{
				const int32 Best = FbOakNear(St, Target->GetActorLocation());
				if (Best >= 0) St.GoalTree = Best;
			}
			MatronCanopyMove(Boss, St, Dt);
		}
		return false;
	}
	// On the ground: the old leg (x0.6), every paw combo leaves her open longer (AttackStarted). Fallen but still with her
	// walkways: she gets up and climbs back after a moment.
	if (Boss->Current == INDEX_NONE) Boss->SpeedBoost = Boss->Tune(TEXT("groundSpeed"), 0.6f);
	if (!St.bGroundForGood && !St.bToBell && Boss->Current == INDEX_NONE && T - St.LandedAt > Boss->Tune(TEXT("climbAfterFall"), 3.f)
		&& !Boss->GetCharacterMovement()->IsFalling() && !Boss->HasTag(WRTags::Status_Staggered))
	{
		const int32 Oak = FbOakNear(St, Boss->GetActorLocation());
		if (Oak >= 0)
		{
			MatronClimb(Boss, St, Oak);
			return true;
		}
	}
	if (Body) Body->bGrounded = St.bGroundForGood;
	return false;
}

bool FWRForestBoss::TickWarden(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	const bool bEnraged = T < St.EnragedUntil;
	if (Body) Body->bEnraged = bEnraged || St.bHunting;
	// Roots hold him: each new hold counted; three inside the window break his stance (stance x2 while held:
	// StanceTaken). Held, he can't move a step (he still turns and swings).
	const bool bHeld = WRForest::IsEntangled(Boss);
	if (bHeld && !St.bWasHeld)
	{
		St.Holds.Add(T);
		const double Window = Boss->Tune(TEXT("holdWindow"), 24.f);
		St.Holds.RemoveAll([T, Window](double H) { return T - H > Window; });
		WRFx::Broadcast(Boss, TEXT("warden.held"), FbFeet(Boss), Boss->GetActorRotation(), Boss->GetSimpleCollisionRadius() / 120.f, TEXT("nature.entangle"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden held by roots (%d in the window)"), St.Holds.Num());
		if (St.Holds.Num() >= FMath::RoundToInt(Boss->Tune(TEXT("holdsToBreak"), 3.f)))
		{
			St.Holds.Reset();
			CancelAttack(Boss);
			UWRCombatLibrary::BreakStance(Boss, UWRCombatLibrary::StanceBreakSeconds(Boss), nullptr);
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden's stance breaks under the roots"));
		}
	}
	St.bWasHeld = bHeld;
	if (bHeld && !Boss->GetCharacterMovement()->IsFalling())
	{
		Boss->GetCharacterMovement()->StopMovementImmediately();
	}
	// Enraged (his herd was hurt): the Bellow every 12 s.
	if (bEnraged && Boss->Current == INDEX_NONE && T >= St.NextBellowAt && T >= Boss->BusyUntil)
	{
		AWRCharacterBase* Target = Boss->PickBossTarget();
		if (Target && Force(Boss, TEXT("bellow"), Target))
		{
			St.NextBellowAt = T + Boss->Tune(TEXT("enragedBellowEvery"), 12.f);
			return true;
		}
	}
	if (Boss->Current == INDEX_NONE)
	{
		Boss->SpeedBoost = bEnraged ? Boss->Tune(TEXT("enrageSpeed"), 1.2f) : 1.f;
	}
	// The Wild Hunt: he never stops running. Unheld, he keeps to the hunt's road and swings only at what is in reach as
	// he passes; roots are the only way to pin him for a fight.
	if (St.bHunting && !bHeld && Boss->Current == INDEX_NONE)
	{
		TickWardenRoute(Boss, St, Dt, true);
		AWRCharacterBase* Near = Boss->PickBossTarget();
		const bool bInReach = Near && FVector::Dist2D(Near->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetSimpleCollisionRadius() + Boss->Tune(TEXT("huntSwingReach"), 700.f);
		return !(bInReach && T >= Boss->BusyUntil);
	}
	return false;
}

bool FWRForestBoss::TickSylvaroth(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	// Phase 1 is rooted from the start of the fight (someone came in before it had settled).
	if (Boss->Phase == 1 && !St.bRooted && Move->IsMovingOnGround())
	{
		FbSetRooted(Boss, St, true, Boss->Tune(TEXT("rootSink"), 600.f));
	}
	// A move cut short (a stance break, a stagger): the root hand's ring sinks back, a re-root ends.
	const FName Doing = Boss->Attacks.IsValidIndex(Boss->Current) ? Boss->Attacks[Boss->Current].Id : NAME_None;
	if (AWRForestProp* Hand = St.HandProp.Get(); Hand && Hand->State == 0 && Doing != TEXT("root_hand"))
	{
		Hand->SetState(3);
	}
	if (St.bRerooting && Doing != TEXT("re_root"))
	{
		St.bRerooting = false;
		FbSetRooted(Boss, St, false, 0.f);
		if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->Pose = 0;
	}
	Heal(Boss, St, Dt);
	if (Boss->Phase >= 2)
	{
		TickSectorsServer(Boss, St, Dt);
	}
	if (Boss->Phase >= 3)
	{
		TickHearts(Boss, St);
		// Phase 3: the canopy shake every 20 s.
		if (Boss->Current == INDEX_NONE && T >= St.NextShakeAt && T >= Boss->BusyUntil)
		{
			AWRCharacterBase* Target = Boss->PickBossTarget();
			if (Target && Force(Boss, TEXT("canopy_shake"), Target))
			{
				St.NextShakeAt = T + Boss->Tune(TEXT("p3ShakeEvery"), 20.f);
				return true;
			}
		}
	}
	return false;
}

void FWRForestBoss::Heal(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	// Each live line heals 0.15% of its max health a second while its root mass touches the ground (all of phase 1;
	// after the Uprooting only while it re-roots). Decay (Necromancy) stops the healing; the Bloom feeds it x1.5.
	if (Boss->HasTag(WRTags::Status_Decay) || !Boss->IsAlive())
	{
		return;
	}
	const bool bFeeding = (Boss->Phase == 1 && St.bRooted) || St.bRerooting;
	if (!bFeeding)
	{
		return;
	}
	const float Now = FbServerNow(Boss->GetWorld());
	int32 Live = 0;
	for (const TWeakObjectPtr<AWRForestRootLine>& L : St.Lines) Live += L.IsValid() && L->IsLive(Now) ? 1 : 0;
	UWRAttributeSet* A = Boss->GetAttributes();
	UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
	if (Live == 0 || !A || !ASC || A->GetHealth() >= A->GetMaxHealth())
	{
		return;
	}
	const float Bloom = WRForest::Bloom(Boss) ? Boss->Tune(TEXT("bloomHealMult"), 1.5f) : 1.f;
	const float Amount = A->GetMaxHealth() * Boss->Tune(TEXT("lineHealPerSecond"), 0.0015f) * Live * Bloom * Dt;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
}

void FWRForestBoss::StartSectors(AWRBoss* Boss, FWRForestBossState& St)
{
	using namespace WRForestBossImpl;
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	if (!Body || Body->Sectors.Num() != 6)
	{
		return;
	}
	// The floor splits into six sectors round its middle; three of them (not one burnt clean) creep for 1.5 s, then live.
	const float Now = FbServerNow(Boss->GetWorld());
	TArray<int32> Free;
	for (int32 i = 0; i < 6; ++i) if (Body->Sectors[i] == 0) Free.Add(i);
	const int32 Want = FMath::Min(Free.Num(), FMath::RoundToInt(Boss->Tune(TEXT("vineSectors"), 3.f)));
	for (int32 n = 0; n < Want; ++n)
	{
		const int32 Pick = FMath::RandRange(0, Free.Num() - 1);
		const int32 S = Free[Pick];
		Free.RemoveAtSwap(Pick);
		Body->Sectors[S] = 1;
		Body->SectorUntil[S] = Now + Boss->Tune(TEXT("vineCreep"), 1.5f);
	}
	WRSound::PlayAt(Boss, TEXT("forest.vine_creep"), St.Home, 1.f);
	UE_LOG(LogWildreach, Log, TEXT("BOSS Sylvaroth's vines creep through %d sectors"), Want);
}

void FWRForestBoss::TickSectorsServer(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	if (!Body || Body->Sectors.Num() != 6)
	{
		return;
	}
	const double T = FbNow(Boss);
	const float Now = FbServerNow(Boss->GetWorld());
	if (T >= St.NextSectorsAt)
	{
		St.NextSectorsAt = T + Boss->Tune(TEXT("vineEvery"), 30.f);
		StartSectors(Boss, St);
	}
	// Fire on the floor burns a sector clean for 10 s (a fire wall, a lava pool, a fire pot's burning ground).
	for (TActorIterator<AWRHazard> It(Boss->GetWorld()); It; ++It)
	{
		const bool bFire = It->Kind == TEXT("fire") || It->Kind == TEXT("lava") || It->bBurnsPieces;
		int32 S = -1;
		if (!bFire || !FbInSector(Body, It->GetActorLocation(), S)) continue;
		if (Body->Sectors[S] == 1 || Body->Sectors[S] == 2)
		{
			Body->Sectors[S] = 3;
			Body->SectorUntil[S] = Now + Boss->Tune(TEXT("vineBurnClear"), 10.f);
			WRFx::Broadcast(Boss, TEXT("sylvaroth.vines_burn"), It->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("forest.thorn_burn"));
		}
	}
	for (int32 i = 0; i < 6; ++i)
	{
		if (Body->Sectors[i] == 0 || Now < Body->SectorUntil[i]) continue;
		if (Body->Sectors[i] == 1)
		{
			Body->Sectors[i] = 2;
			Body->SectorUntil[i] = Now + Boss->Tune(TEXT("vineLive"), 12.f);
		}
		else
		{
			Body->Sectors[i] = 0;
		}
	}
	// Standing in a live sector a second: Entangled (the vines curl round your feet).
	TSet<TWeakObjectPtr<AActor>> Seen;
	for (AWRCharacterBase* Who : FbFoes(Boss, St.ArenaR + 500.f, St.Home))
	{
		int32 S = -1;
		if (!FbInSector(Body, Who->GetActorLocation(), S) || Body->Sectors[S] != 2 || Who->GetCharacterMovement()->IsFalling()) continue;
		Seen.Add(Who);
		float& Stood = St.SectorStand.FindOrAdd(Who);
		Stood += Dt;
		if (Stood >= Boss->Tune(TEXT("vineStandSeconds"), 1.f))
		{
			Stood = 0.f;
			WRForest::Entangle(Who, Boss->Tune(TEXT("vineEntangle"), 1.f), Boss);
		}
	}
	for (auto It = St.SectorStand.CreateIterator(); It; ++It) if (!Seen.Contains(It->Key)) It.RemoveCurrent();
}

void FWRForestBoss::TickHearts(AWRBoss* Boss, FWRForestBossState& St)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	// Every awake heart-tree swings its lower limbs at anyone within 15 m, every 6 s (a 0.9 s tell: the limbs lift).
	for (int32 i = 0; i < St.Hearts.Num() && i < 4; ++i)
	{
		AWRForestProp* H = St.Hearts[i].Get();
		if (!H || H->State != 1) continue;
		if (St.NextHeartSweep[i] <= 0.0) St.NextHeartSweep[i] = T + 1.5 * i + 2.0;
		if (T >= St.NextHeartSweep[i])
		{
			St.NextHeartSweep[i] = T + Boss->Tune(TEXT("p3SweepEvery"), 6.f);
			St.HeartHitAt[i] = T + Boss->Tune(TEXT("p3SweepWindup"), 0.9f);
			H->SweepAt = FbServerNow(Boss->GetWorld());
			H->ForceNetUpdate();
			WRSound::PlayAt(Boss, TEXT("forest.root_pulse"), H->GetActorLocation(), 1.f);
		}
		if (St.HeartHitAt[i] > 0.0 && T >= St.HeartHitAt[i])
		{
			St.HeartHitAt[i] = 0.0;
			const FVector Foot = H->GetActorLocation();
			WRFx::Broadcast(Boss, TEXT("sylvaroth.limb_sweep"), Foot, H->GetActorRotation(), Boss->Tune(TEXT("p3SweepRadius"), 1500.f) / 500.f, TEXT("forest.uproot"));
			for (AWRCharacterBase* Who : FbFoes(Boss, Boss->Tune(TEXT("p3SweepRadius"), 1500.f), Foot))
			{
				FbHitWith(Boss, Who, Boss->Tune(TEXT("p3SweepDamage"), 40.f), 80.f, 4, false, true, Foot);
			}
		}
	}
}

void FWRForestBoss::TickWaves(AWRBoss* Boss, FWRForestBossState& St)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	const float Depth = WRTune(TEXT("bosses.waveDepth"), 180.f);
	for (int32 w = St.Waves.Num() - 1; w >= 0; --w)
	{
		FWRForestBossWave& Wave = St.Waves[w];
		const float Front = float(T - Wave.Start) * Wave.Speed;
		if (Front > Wave.MaxR + Depth)
		{
			St.Waves.RemoveAt(w);
			continue;
		}
		for (AWRCharacterBase* Who : FbFoes(Boss, Wave.MaxR, Wave.At))
		{
			if (Wave.Done.Contains(Who)) continue;
			const float D = FVector::Dist2D(Who->GetActorLocation(), Wave.At) - Who->GetSimpleCollisionRadius();
			if (D <= Front && D >= Front - Depth)
			{
				Wave.Done.Add(Who);
				FbHitWith(Boss, Who, Wave.Damage, 60.f, 3, false, Wave.bLow, Wave.At);
			}
		}
	}
}

void FWRForestBoss::Grab(AWRBoss* Boss, FWRForestBossState& St, AWRCharacterBase* Victim, FName How)
{
	using namespace WRForestBossImpl;
	if (!Victim || !Victim->IsAlive() || !Boss->IsAlive())
	{
		return;
	}
	ReleaseGrab(Boss, St, false);
	const double T = FbNow(Boss);
	const bool bHand = How == TEXT("root_hand");
	const float Seconds = bHand ? Boss->Tune(TEXT("rootHandSeconds"), 3.f) : Boss->Tune(TEXT("graspSeconds"), 2.5f);
	St.Hold.Who = Victim;
	St.Hold.How = How;
	St.Hold.Start = T;
	St.Hold.Ends = T + Seconds;
	St.Hold.TickAt = T + 0.5;
	St.Hold.Dps = bHand ? Boss->Tune(TEXT("rootHandDps"), 20.f) : Boss->Tune(TEXT("graspDps"), 15.f);
	St.Hold.ArmDamage = 0.f;
	St.Hold.Hand = bHand ? St.HandProp : TWeakObjectPtr<AWRForestProp>();
	// Held off the ground: no move, no roll; collision off so the hold carries them (the swamp's grabs do the same).
	Victim->SetActorEnableCollision(false);
	Victim->GetCharacterMovement()->DisableMovement();
	if (AWRForestProp* Hand = St.Hold.Hand.Get())
	{
		Victim->AttachToActor(Hand, FAttachmentTransformRules::KeepWorldTransform);
		Victim->SetActorRelativeLocation(FVector(0.f, 0.f, Boss->Tune(TEXT("handLift"), 420.f)));
		Hand->Held = Victim;
	}
	else
	{
		// In the right fist of knotted branches, held up at its shoulder.
		Victim->AttachToActor(Boss, FAttachmentTransformRules::KeepWorldTransform);
		Victim->SetActorRelativeLocation(FVector(Boss->GetSimpleCollisionRadius() * 1.1f, Boss->GetSimpleCollisionRadius() * 0.55f, Boss->GetSimpleCollisionHalfHeight() * 0.25f));
	}
	if (Victim->Status) Victim->Status->Stagger(Seconds + 0.4f);
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike)
	{
		Boss->StrikeEnds = FMath::Max(Boss->StrikeEnds, St.Hold.Ends);
	}
	WRFx::Broadcast(Boss, bHand ? TEXT("sylvaroth.root_erupt") : TEXT("gnarl.grasp"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("forest.uproot"));
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s %s holds %s (%.1f s)"), *Boss->GetDisplayLabel(), *How.ToString(), *Victim->GetName(), Seconds);
}

void FWRForestBoss::ReleaseGrab(AWRBoss* Boss, FWRForestBossState& St, bool bThrow)
{
	using namespace WRForestBossImpl;
	AWRCharacterBase* V = St.Hold.Who.Get();
	const FName How = St.Hold.How;
	AWRForestProp* Hand = St.Hold.Hand.Get();
	St.Hold = FWRForestBossHold();
	if (Hand)
	{
		Hand->Held = nullptr;
		if (Hand->State != 2) Hand->SetState(3);
	}
	if (!V)
	{
		return;
	}
	V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	V->SetActorEnableCollision(true);
	V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive() && Boss)
	{
		if (How == TEXT("root_hand"))
		{
			// Slammed down onto the moss.
			FbHitWith(Boss, V, Boss->Tune(TEXT("rootHandSlam"), 40.f), 150.f, 5, true, false, V->GetActorLocation() + FVector(0.f, 0.f, 200.f));
			V->LaunchCharacter(FVector(0.f, 0.f, -1500.f), true, true);
			WRFx::Broadcast(Boss, TEXT("land.dust"), FbFeet(V), FRotator::ZeroRotator, 1.6f, TEXT("stomp"));
		}
		else
		{
			// Thrown 8 m.
			const float K = Boss->Tune(TEXT("graspThrow"), 800.f);
			V->LaunchCharacter((V->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D() * K + FVector(0.f, 0.f, K * 0.5f), true, true);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s lets %s go (%s)"), Boss ? *Boss->GetDisplayLabel() : TEXT("?"), *V->GetName(), bThrow ? TEXT("thrown") : TEXT("freed"));
}

void FWRForestBoss::TickHolds(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	AWRCharacterBase* V = St.Hold.Who.Get();
	if (!V)
	{
		if (!St.Hold.How.IsNone()) St.Hold = FWRForestBossHold();
		return;
	}
	const double T = FbNow(Boss);
	AWRForestProp* Hand = St.Hold.Hand.Get();
	// The wrist torn (600 by allies) or the arm hit for 100: dropped, no slam, no throw.
	if (!V->IsAlive() || !Boss->IsAlive() || (St.Hold.How == TEXT("root_hand") && (!Hand || Hand->State == 2)))
	{
		ReleaseGrab(Boss, St, false);
		return;
	}
	if (St.Hold.How == TEXT("grasp") && St.Hold.ArmDamage >= Boss->Tune(TEXT("graspFreeDamage"), 100.f))
	{
		WRFx::Broadcast(Boss, TEXT("gnarl.bark_chips"), V->GetActorLocation(), FRotator::ZeroRotator, 1.2f, TEXT("forest.bark_strip"));
		ReleaseGrab(Boss, St, false);
		return;
	}
	if (T >= St.Hold.TickAt)
	{
		St.Hold.TickAt = T + 0.5;
		FbSqueeze(Boss, V, St.Hold.Dps * 0.5f);
	}
	if (T >= St.Hold.Ends)
	{
		ReleaseGrab(Boss, St, true);
	}
	(void)Dt;
}

// ---------------------------------------------------------------- the Grey Matron's canopy

void FWRForestBoss::MatronCanopyMove(AWRBoss* Boss, FWRForestBossState& St, float Dt)
{
	using namespace WRForestBossImpl;
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
	Move->MaxFlySpeed = 0.f;
	Move->StopMovementImmediately();
	const float HH = Boss->GetSimpleCollisionHalfHeight();
	if (St.Walk < 0 && St.GoalTree >= 0 && St.GoalTree != St.Tree)
	{
		const int32 W = FbNextWalk(St, St.Tree, St.GoalTree);
		if (W == INDEX_NONE)
		{
			St.GoalTree = -1;
		}
		else
		{
			St.Walk = W;
			St.bForward = St.Walks[W]->TreeA == St.Tree;
			St.WalkT = St.bForward ? 0.06f : 0.94f;
		}
	}
	if (St.Walk >= 0)
	{
		AWRForestWalkway* Wk = St.Walks.IsValidIndex(St.Walk) ? St.Walks[St.Walk].Get() : nullptr;
		if (!Wk || Wk->bDown)
		{
			St.Walk = -1;
			return;
		}
		// Along the planks at a prowl (the walkway sags under her: she follows its curve).
		const float Len = FMath::Max(100.f, FVector::Dist(FVector(Wk->EndA), FVector(Wk->EndB)));
		const float Step = Boss->Tune(TEXT("canopySpeed"), 650.f) * Dt / Len;
		St.WalkT = FMath::Clamp(St.WalkT + (St.bForward ? Step : -Step), 0.06f, 0.94f);
		const FVector P = Wk->DeckAt(St.WalkT);
		const FVector Ahead = Wk->DeckAt(FMath::Clamp(St.WalkT + (St.bForward ? 0.02f : -0.02f), 0.f, 1.f));
		Boss->SetActorLocation(P + FVector(0.f, 0.f, HH + 4.f), false, nullptr, ETeleportType::None);
		Boss->SetActorRotation(FRotator(0.f, (Ahead - P).Rotation().Yaw, 0.f));
		if ((St.bForward && St.WalkT >= 0.94f) || (!St.bForward && St.WalkT <= 0.06f))
		{
			St.Tree = St.bForward ? Wk->TreeB : Wk->TreeA;
			St.Walk = -1;
			if (St.Tree == St.GoalTree) St.GoalTree = -1;
		}
		return;
	}
	// Perched: crouched on the platform's lip, watching.
	FVector Perch;
	if (FbPerch(St, St.Tree, Perch))
	{
		const FVector Want = Perch + FVector(0.f, 0.f, HH + 4.f);
		if (!Boss->GetActorLocation().Equals(Want, 5.f)) Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Want, Dt, 600.f));
	}
}

void FWRForestBoss::MatronDrop(AWRBoss* Boss, FWRForestBossState& St, bool bForGood)
{
	using namespace WRForestBossImpl;
	St.bCanopy = false;
	St.Walk = -1;
	St.GoalTree = -1;
	St.bClimbing = false;
	St.LandedAt = FbNow(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	Move->Velocity = FVector::ZeroVector;
	if (bForGood)
	{
		St.bGroundForGood = true;
		if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->bGrounded = true;
		WRSound::PlayAt(Boss, TEXT("forest.walkway_fall"), Boss->GetActorLocation(), 1.f);
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Grey Matron comes down for good"));
	}
	else
	{
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Grey Matron falls"));
	}
}

void FWRForestBoss::MatronClimb(AWRBoss* Boss, FWRForestBossState& St, int32 TreeIndex)
{
	using namespace WRForestBossImpl;
	FVector Perch;
	if (!St.Oaks.IsValidIndex(TreeIndex) || !FbPerch(St, TreeIndex, Perch))
	{
		return;
	}
	// A bound to the trunk's foot, then up the bark to the platform (the claws scraping: she can be hit on the way).
	const FVector Oak = St.Oaks[TreeIndex];
	const FVector Side = (Boss->GetActorLocation() - Oak).GetSafeNormal2D();
	const float HH = Boss->GetSimpleCollisionHalfHeight();
	St.ClimbFrom = Boss->GetActorLocation();
	St.ClimbBase = FVector(Oak.X, Oak.Y, Oak.Z) + Side * FbOakRadius(Boss) + FVector(0.f, 0.f, HH + 120.f);
	St.ClimbTo = Perch + FVector(0.f, 0.f, HH + 4.f);
	St.ClimbTree = TreeIndex;
	St.ClimbStart = FbNow(Boss);
	St.ClimbEnd = St.ClimbStart + FVector::Dist2D(St.ClimbFrom, St.ClimbBase) / 900.f + Boss->Tune(TEXT("climbSeconds"), 2.5f);
	St.bClimbing = true;
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Flying);
	Move->MaxFlySpeed = 0.f;
	Move->StopMovementImmediately();
	if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->Pose = 2;
	WRSound::PlayAt(Boss, TEXT("forest.canopy_rustle"), St.ClimbBase, 1.f);
}

bool FWRForestBoss::TickMatronClimb(AWRBoss* Boss, FWRForestBossState& St)
{
	using namespace WRForestBossImpl;
	const double T = FbNow(Boss);
	const float Total = FMath::Max(0.3f, float(St.ClimbEnd - St.ClimbStart));
	const float Run = FMath::Min(Total * 0.4f, FVector::Dist2D(St.ClimbFrom, St.ClimbBase) / 900.f);
	const float A = float(T - St.ClimbStart);
	FVector P;
	if (A < Run)
	{
		P = FMath::Lerp(St.ClimbFrom, St.ClimbBase, FMath::Clamp(A / FMath::Max(0.05f, Run), 0.f, 1.f));
	}
	else
	{
		const float U = FMath::Clamp((A - Run) / FMath::Max(0.1f, Total - Run), 0.f, 1.f);
		// Straight up the trunk, then over onto the platform at the top.
		const FVector Up(St.ClimbBase.X, St.ClimbBase.Y, St.ClimbTo.Z);
		P = U < 0.85f ? FMath::Lerp(St.ClimbBase, Up, U / 0.85f) : FMath::Lerp(Up, St.ClimbTo, (U - 0.85f) / 0.15f);
	}
	Boss->SetActorLocation(P, false, nullptr, ETeleportType::None);
	const FVector Face = (FVector(St.Oaks[St.ClimbTree]) - P).GetSafeNormal2D();
	if (!Face.IsNearlyZero()) Boss->SetActorRotation(FRotator(0.f, Face.Rotation().Yaw, 0.f));
	if (T >= St.ClimbEnd)
	{
		St.bClimbing = false;
		St.bCanopy = true;
		St.Tree = St.ClimbTree;
		St.Walk = -1;
		if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) { Body->Pose = 0; Body->bGrounded = false; }
	}
	return true;
}

// ---------------------------------------------------------------- the Hornwarden's loop and the Wild Hunt

bool FWRForestBoss::TickWardenRoute(AWRBoss* Boss, FWRForestBossState& St, float Dt, bool bFighting)
{
	using namespace WRForestBossImpl;
	UWorld* World = Boss->GetWorld();
	const double T = FbNow(Boss);
	// The Bloom's night is the Wild Hunt's (if the hazards' event step hasn't called it, his own clock does).
	const bool bHuntNight = WRForest::Bloom(Boss) && AWRGameState::IsNightIn(Boss);
	if (!bFighting && bHuntNight != WRForest::WildHuntActive(Boss))
	{
		SetWildHunt(World, bHuntNight);
	}
	const bool bHunt = WRForest::WildHuntActive(Boss);
	if (bHunt != St.bHunting)
	{
		St.bHunting = bHunt;
		St.Route = -1;
		if (UWRForestBossBody* Body = UWRForestBossBody::Get(Boss)) Body->bEnraged = bHunt;
		if (bHunt) WRFx::BroadcastOn(Boss, TEXT("warden.hunt_mist"), NAME_None, 2.f, TEXT("forest.hunt_horn"), 6.f);
	}
	// His herd: six Elderelk (ten in the Hunt), made once, around him.
	St.Herd.RemoveAll([](const TWeakObjectPtr<AWRCreature>& H) { return !H.IsValid() || !H->IsAlive() || H->IsTamed(); });
	const int32 WantHerd = FMath::RoundToInt(bHunt ? Boss->Tune(TEXT("huntHerd"), 10.f) : Boss->Tune(TEXT("herd"), 6.f));
	if ((!St.bHerdMade || (bHunt && !St.bHuntHerd)) && !bFighting)
	{
		St.bHerdMade = true;
		St.bHuntHerd |= bHunt;
		for (int32 i = St.Herd.Num(); i < WantHerd; ++i)
		{
			FVector Spot;
			if (!FbGround(World, Boss->GetActorLocation() + FRotator(0.f, 360.f * i / WantHerd + 20.f, 0.f).Vector() * FMath::FRandRange(800.f, 1400.f), Spot)) continue;
			if (AWRCreature* Elk = FbSpawnRow(World, TEXT("elderelk"), Spot + FVector(0.f, 0.f, 200.f), Boss->GetActorRotation()))
			{
				St.Herd.Add(Elk);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden walks with %d Elderelk%s"), St.Herd.Num(), bHunt ? TEXT(" (the Wild Hunt)") : TEXT(""));
	}
	const FVector2D Heading = FVector2D(Boss->GetVelocity()).GetSafeNormal();
	for (const TWeakObjectPtr<AWRCreature>& H : St.Herd)
	{
		if (H->GetAggroTarget()) continue;
		H->FollowPack(FVector2D(Boss->GetActorLocation()), Heading);
		H->SpeedBoost = bHunt ? Boss->Tune(TEXT("huntSpeed"), 1.6f) : 1.f;
	}
	const TArray<FVector> Pts = FbRouteOf(Boss, bHunt ? FName(TEXT("hunt")) : FName(TEXT("roam")));
	if (Pts.Num() < 2)
	{
		return false;
	}
	// His day (not in the Hunt): he rests at the Antler Barrow at noon, stands under the Hanging Oak at dusk and drinks at
	// Herne's Ford as night falls, when his road brings him there in that hour.
	if (!bHunt && !bFighting)
	{
		const float Hour = AWRGameState::HourIn(Boss);
		struct FStop { const TCHAR* Site; float From, To; };
		static const FStop Stops[] = { { TEXT("antler_barrow"), 11.f, 13.5f }, { TEXT("hanging_oak"), 18.f, 19.5f }, { TEXT("hernes_ford"), 20.f, 21.5f } };
		if (T < St.RestUntil)
		{
			Boss->GetCharacterMovement()->StopMovementImmediately();
			return true;
		}
		for (const FStop& S : Stops)
		{
			FVector At;
			if (Hour >= S.From && Hour < S.To && T - St.RestedAt > 600.0 && FbSiteOfKind(S.Site, At) && FVector::Dist2D(At, Boss->GetActorLocation()) < Boss->Tune(TEXT("stopRadius"), 3000.f))
			{
				St.RestedAt = T;
				St.RestUntil = T + Boss->Tune(TEXT("stopSeconds"), 180.f);
				Boss->SetHome(FVector(Boss->GetActorLocation()));
				UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden stops at %s"), S.Site);
				return true;
			}
		}
	}
	if (St.Route < 0 || !Pts.IsValidIndex(St.Route))
	{
		// Join his road at its nearest point.
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const float D = FVector::DistSquared2D(Pts[i], Boss->GetActorLocation());
			if (D < Best) { Best = D; St.Route = i; }
		}
	}
	// The Hunt never runs through a safe zone: those points are passed by.
	for (int32 Guard = 0; Guard < Pts.Num() && bHunt && FWRSocialRules::IsInSafeZone(World, Pts[St.Route]); ++Guard)
	{
		St.Route = (St.Route + 1) % Pts.Num();
	}
	const FVector Goal = Pts[St.Route];
	const FVector To = Goal - Boss->GetActorLocation();
	if (To.Size2D() < 400.f)
	{
		St.Route = (St.Route + 1) % Pts.Num();
		return true;
	}
	if (!bFighting)
	{
		Boss->SetHome(FVector(Goal.X, Goal.Y, Boss->GetActorLocation().Z));
		Boss->bReturning = false;
	}
	Boss->SpeedBoost = bHunt ? Boss->Tune(TEXT("huntSpeed"), 1.6f) : 1.f;
	Boss->AddMovementInput(To.GetSafeNormal2D(), bHunt ? 1.f : WRTune(TEXT("bosses.roamInput"), 0.45f));
	(void)Dt;
	return true;
}

// ---------------------------------------------------------------- the attack machine's hooks

AWRCharacterBase* FWRForestBoss::PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || !Nearest)
	{
		return Nearest;
	}
	// The Grey Matron in her canopy picks by the ground under her (who her drops can reach), not by the climb down.
	if (Boss->CreatureId == TEXT("grey_matron") && FbS(Boss).bCanopy)
	{
		AWRCharacterBase* Best = Nearest;
		float BestD = TNumericLimits<float>::Max();
		for (AWRCharacterBase* Who : FbFoes(Boss, Boss->GetChaseRadius(), Boss->GetHome()))
		{
			const float D = FVector::Dist2D(Who->GetActorLocation(), Boss->GetActorLocation());
			if (D < BestD) { BestD = D; Best = Who; }
		}
		return Best;
	}
	return Nearest;
}

bool FWRForestBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || !Target)
	{
		return true;
	}
	const FWRForestBossState* St = FbFind(Boss);
	if (!St)
	{
		return true;
	}
	const FName Id = Attack.Id;
	const float D2 = FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation());
	// Moves its own rules start, never picked (the regrow, the swarm, the screams).
	if (Id == TEXT("regrow_bark") || Id == TEXT("swarm_call") || Id == TEXT("kit_scream"))
	{
		return false;
	}
	if (St->Hold.Who.IsValid() && (Attack.bGrab || Id == TEXT("boulder") || Id == TEXT("root_hand")))
	{
		return false;   // one in the hand at a time
	}
	const FName K = St->Kind;
	if (K == TEXT("old_gnarl"))
	{
		// Never thrown inside 8 m; one stump in the air at a time.
		if (Id == TEXT("boulder")) return D2 >= Boss->Tune(TEXT("boulderMin"), 800.f) && St->StumpsInFlight == 0;
	}
	else if (K == TEXT("bramblequeen"))
	{
		if (Id == TEXT("pheromone_mark")) return !WRForest::HasWaspMark(Target);
	}
	else if (K == TEXT("grey_matron"))
	{
		const bool bCanopyMove = Id == TEXT("canopy_drop") || Id == TEXT("bridge_leap");
		if (bCanopyMove)
		{
			if (!St->bCanopy || St->bGroundForGood || St->Walk >= 0) return false;
			if (Id == TEXT("canopy_drop")) return Boss->GetActorLocation().Z - Target->GetActorLocation().Z > 500.f;
			return true;
		}
		// Her bite and paws are for the ground (or a survivor up on her own walkway, nose to nose).
		if (St->bCanopy) return FMath::Abs(Boss->GetActorLocation().Z - Target->GetActorLocation().Z) < 300.f && St->Walk < 0;
	}
	else if (K == TEXT("hornwarden"))
	{
		if (Id == TEXT("antler_charge")) return !WRForest::IsEntangled(Boss);
		// One antler broken: the toss is gone.
		if (Id == TEXT("antler_toss")) return !Boss->IsPartBroken(TEXT("antler_left")) && !Boss->IsPartBroken(TEXT("antler_right"));
		if (Id == TEXT("bellow")) return St->Herd.Num() > 0;
	}
	else if (K == TEXT("sylvaroth"))
	{
		if (Id == TEXT("seed_bombs"))
		{
			const int32 Cap = FMath::RoundToInt(Boss->Phase >= 2 ? WRTune(TEXT("bosses.projectilesInFlight.p2"), 2.f) : WRTune(TEXT("bosses.projectilesInFlight.p1"), 1.f));
			const int32 Pods = FMath::Max(1, FMath::RoundToInt(Boss->Tune(TEXT("seedPods"), 6.f)));
			return (St->PodsInFlight + Pods - 1) / Pods < Cap;
		}
		if (Id == TEXT("thorn_whip")) return FbFurthestTame(Boss) || FbFurthestPlant(Boss);
		if (Id == TEXT("re_root"))
		{
			const float Now = FbServerNow(Boss->GetWorld());
			int32 Live = 0;
			for (const TWeakObjectPtr<AWRForestRootLine>& L : St->Lines) Live += L.IsValid() && L->IsLive(Now) ? 1 : 0;
			return Live > 0 && FbHealthFraction(Boss) < Boss->Tune(TEXT("rerootBelow"), 0.45f);
		}
		if (Id == TEXT("knuckle_charge")) return D2 > Boss->Tune(TEXT("chargeMin"), 1200.f);
	}
	return true;
}

void FWRForestBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	const double T = FbNow(Boss);
	const FName Id = Attack.Id;
	const FName K = St.Kind;
	const FVector Feet = FbFeet(Boss);
	const float R = Boss->GetSimpleCollisionRadius();
	// Punish windows: the Hornwarden's enrage cuts them (x0.7); the Matron's every paw combo on the ground leaves 1.2 s more.
	if (Attack.bRow)
	{
		float Punish = St.BasePunish.Contains(Id) ? St.BasePunish[Id] : Attack.Row.Punish;
		if (K == TEXT("hornwarden") && T < St.EnragedUntil) Punish *= Boss->Tune(TEXT("enragePunish"), 0.7f);
		if (K == TEXT("grey_matron") && !St.bCanopy && Id.ToString().StartsWith(TEXT("paw"))) Punish += Boss->Tune(TEXT("groundPunish"), 1.2f);
		Attack.Row.Punish = Punish;
	}
	// Sylvaroth walking: its first phase's moves come quicker (x0.9).
	if (K == TEXT("sylvaroth") && Boss->Phase >= 2 && FbIsRootedMove(Id))
	{
		Boss->ResolveAt = Boss->StrikeStarted + (Boss->ResolveAt - Boss->StrikeStarted) * Boss->Tune(TEXT("p2TimingScale"), 0.9f);
	}
	if (K == TEXT("old_gnarl"))
	{
		if (Id == TEXT("boulder"))
		{
			// It tears a stump and its root ball out of the ground beside it, soil pouring off.
			WRFx::Broadcast(Boss, TEXT("gnarl.stump_tear"), Feet + Boss->GetActorRightVector() * R * 0.9f, Boss->GetActorRotation(), 1.5f, TEXT("forest.uproot"));
		}
		else if (Id == TEXT("regrow_bark") && St.RegrowPlate >= 0)
		{
			// Sap bubbles out of the stripped spot and bark crawls over it.
			if (Body) Body->Pose = 1;
			WRFx::Broadcast(Boss, TEXT("gnarl.sap_bubble"), FbPlatePoint(Boss, St.RegrowPlate), Boss->GetActorRotation(), 1.2f, TEXT("forest.bark_regrow"), Attack.Windup);
		}
		else if (Id == TEXT("root_slam"))
		{
			WRFx::Broadcast(Boss, TEXT("gnarl.soil_pour"), Feet, Boss->GetActorRotation(), R / 150.f, NAME_None, Attack.Windup);
		}
	}
	else if (K == TEXT("bramblequeen"))
	{
		if (Body) Body->Pose = Id == TEXT("sting_lance") ? 1 : Id == TEXT("pheromone_mark") ? 2 : Id == TEXT("swarm_call") ? 3 : Id == TEXT("wing_gust") ? 4 : 0;
		if (Id == TEXT("pheromone_mark"))
		{
			WRFx::Broadcast(Boss, TEXT("queen.pheromone_fan"), Boss->GetActorLocation() - Boss->GetActorForwardVector() * R, Boss->GetActorRotation(), 1.f, TEXT("forest.wasp_swarm"), Attack.Windup);
		}
		if (Id == TEXT("swarm_call"))
		{
			// The living comb nearest her: she flies over to it and bows.
			St.SwarmComb = -1;
			float Best = TNumericLimits<float>::Max();
			for (int32 i = 0; i < St.Combs.Num(); ++i)
			{
				const AWRForestProp* C = St.Combs[i].Get();
				if (!C || C->State != 0) continue;
				const float D = FVector::Dist(C->GetActorLocation(), Boss->GetActorLocation());
				if (D < Best) { Best = D; St.SwarmComb = i; }
			}
			WRSound::PlayAt(Boss, TEXT("forest.wasp_swarm"), Boss->GetActorLocation(), 1.2f);
		}
	}
	else if (K == TEXT("grey_matron"))
	{
		if (Id == TEXT("canopy_drop"))
		{
			// Bark and leaves rain from where she hangs; her shadow grows on the moss where she'll land.
			WRFx::Broadcast(Boss, TEXT("matron.bark_rain"), Boss->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("forest.canopy_rustle"), Attack.Windup);
			AWRGroundMarker::Spawn(Boss->GetWorld(), Boss->AttackPoint + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector2D(Attack.Size, Attack.Size), false, FLinearColor(0.012f, 0.012f, 0.016f), Attack.Windup + Attack.Row.Active + 0.1f);
		}
		else if (Id == TEXT("bridge_leap"))
		{
			// The oak across whose line runs over whoever she's after (her shadow will sweep across them).
			FVector From;
			FbPerch(St, St.Tree, From);
			St.LeapTo = -1;
			float Best = TNumericLimits<float>::Max();
			for (int32 Oak = 0; Oak < St.Oaks.Num(); ++Oak)
			{
				FVector P;
				if (Oak == St.Tree || !FbPerch(St, Oak, P)) continue;
				const float D = FbSegDist2D(Target->GetActorLocation(), From, P) - FVector::Dist2D(From, P) * 0.02f;
				if (D < Best) { Best = D; St.LeapTo = Oak; St.LeapEnd = P; }
			}
			St.LeapFrom = From;
			if (Body) Body->Pose = 1;
			if (St.LeapTo >= 0)
			{
				const FVector Mid = (St.LeapFrom + St.LeapEnd) * 0.5f;
				FVector G;
				FbGround(Boss->GetWorld(), Mid, G);
				AWRGroundMarker::Spawn(Boss->GetWorld(), G + FVector(0.f, 0.f, 3.f), (St.LeapEnd - St.LeapFrom).Rotation(), FVector2D(FVector::Dist2D(St.LeapFrom, St.LeapEnd), 220.f), true,
					FLinearColor(0.012f, 0.012f, 0.016f), Attack.Windup + Attack.Row.Hold + 0.2f);
			}
		}
		else if (Id == TEXT("kit_scream"))
		{
			WRFx::BroadcastOn(Boss, TEXT("matron.scream"), TEXT("head"), 1.f, TEXT("forest.kit_scream"));
		}
	}
	else if (K == TEXT("hornwarden"))
	{
		if (Id == TEXT("antler_charge"))
		{
			WRFx::Broadcast(Boss, TEXT("warden.hoof_scrape"), Feet + Boss->GetActorForwardVector() * R * 0.6f, Boss->GetActorRotation(), 1.f, TEXT("forest.hunt_hooves"), Attack.Windup);
		}
		else if (Id == TEXT("bellow"))
		{
			if (Body) Body->Pose = 1;
		}
	}
	else if (K == TEXT("sylvaroth"))
	{
		if (Id == TEXT("root_eruption"))
		{
			// The ground bulges in a line toward you, moss tearing, a crackle running along it.
			St.EruptDir = (Boss->AttackPoint - Feet).GetSafeNormal2D();
			if (St.EruptDir.IsNearlyZero()) St.EruptDir = Boss->GetActorForwardVector();
			St.EruptFrom = Feet + St.EruptDir * R;
			const float Len = Attack.Range;
			for (int32 i = 0; i < 6; ++i)
			{
				FVector G;
				FbGround(Boss->GetWorld(), St.EruptFrom + St.EruptDir * Len * (i + 0.5f) / 6.f, G);
				WRFx::Broadcast(Boss, TEXT("sylvaroth.ground_bulge"), G, St.EruptDir.Rotation(), Len / 600.f / 6.f * 1.2f, i == 0 ? FName(TEXT("forest.uproot")) : NAME_None, Attack.Windup + Attack.Row.Hold);
			}
		}
		else if (Id == TEXT("seed_bombs"))
		{
			// Its back canopy shakes; six pods swell orange-brown.
			if (Body) Body->Pose = 3;
			WRFx::BroadcastOn(Boss, TEXT("sylvaroth.pods_swell"), TEXT("spine"), R / 300.f, TEXT("forest.seed_pod"));
		}
		else if (Id == TEXT("root_hand"))
		{
			// One arm sinks into the earth; under the target a 3 m ring trembles with a crimson rim (dodge out of it).
			if (AWRForestProp* Old = St.HandProp.Get()) Old->Destroy();
			FVector G;
			FbGround(Boss->GetWorld(), Boss->AttackPoint, G);
			const FTransform Where(FRotator::ZeroRotator, G);
			if (AWRForestProp* Hand = Boss->GetWorld()->SpawnActorDeferred<AWRForestProp>(AWRForestProp::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Hand->Kind = TEXT("root_hand");
				Hand->Size = Attack.Size;
				Hand->OwnerBoss = Boss;
				Hand->FinishSpawning(Where);
				St.HandProp = Hand;
			}
			if (Body) Body->Pose = 4;
		}
		else if (Id == TEXT("canopy_shake"))
		{
			if (Body) Body->Pose = 1;
			WRFx::BroadcastOn(Boss, TEXT("sylvaroth.canopy_shed"), TEXT("spine"), R / 300.f, TEXT("forest.canopy_rustle"), Attack.Windup + Attack.Row.Active);
		}
		else if (Id == TEXT("uprooting"))
		{
			// The whole clearing heaves: a crimson-rimmed ring 20 m round its body cracks open.
			WRFx::Broadcast(Boss, TEXT("sylvaroth.uproot_ring"), Feet + FVector(0.f, 0.f, St.SinkNow), FRotator::ZeroRotator, Attack.Size / 1000.f, TEXT("forest.uproot"), Attack.Windup);
		}
		else if (Id == TEXT("re_root"))
		{
			if (Body) Body->Pose = 2;
		}
		else if (Id == TEXT("thorn_whip"))
		{
			// The furthest tame (the plant summons only when there's no tame).
			AWRCharacterBase* Tame = FbFurthestTame(Boss);
			St.WhipAt = Tame ? static_cast<AWRCharacterBase*>(Tame) : FbFurthestPlant(Boss);
			if (AWRCharacterBase* W = St.WhipAt.Get())
			{
				Boss->AttackPoint = FbFeet(W);
				WRFx::Broadcast(Boss, TEXT("sylvaroth.vine_uncoil"), Boss->GetActorLocation(), (W->GetActorLocation() - Boss->GetActorLocation()).Rotation(), FVector::Dist(W->GetActorLocation(), Boss->GetActorLocation()) / 1000.f, TEXT("nature.vine_lash"), Attack.Windup);
			}
		}
	}
}

bool FWRForestBoss::TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	FWRForestBossState& St = FbS(Boss);
	if (St.Kind == TEXT("bramblequeen") && Attack.Id == TEXT("swarm_call") && St.Combs.IsValidIndex(St.SwarmComb))
	{
		// Over to the comb: she hangs beside it, bowed over it.
		if (const AWRForestProp* C = St.Combs[St.SwarmComb].Get())
		{
			const FVector Out = (Boss->GetActorLocation() - C->GetActorLocation()).GetSafeNormal2D();
			const FVector Want = C->GetActorLocation() + Out * (Boss->GetSimpleCollisionRadius() + 150.f) + FVector(0.f, 0.f, 40.f);
			Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Want, DeltaSeconds, 900.f), true);
			Boss->SetActorRotation(FRotator(0.f, (-Out).Rotation().Yaw, 0.f));
		}
		return true;
	}
	if (St.Kind == TEXT("sylvaroth") && Attack.Id == TEXT("root_hand"))
	{
		return true;   // the ring stays where it opened
	}
	return false;
}

bool FWRForestBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	FWRForestBossState& St = FbS(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	UWorld* World = Boss->GetWorld();
	const double T = FbNow(Boss);
	const FName Id = Attack.Id;
	const FName K = St.Kind;
	const float R = Boss->GetSimpleCollisionRadius(), HH = Boss->GetSimpleCollisionHalfHeight();
	const FVector Fwd = Boss->AttackFacing.Vector().GetSafeNormal2D();
	AWRCharacterBase* Target = Boss->AttackTarget.Get();

	if (K == TEXT("old_gnarl"))
	{
		if (Id == TEXT("boulder"))
		{
			// The stump and its root ball fly from its hand on a gravity arc to where you stood (a stump in the way stops
			// it); its shadow grows there from the throw.
			const FVector From = Boss->GetActorLocation() + Boss->GetActorRightVector() * R * 0.8f + FVector(0.f, 0.f, HH * 0.75f);
			const float Dist = FVector::Dist2D(From, Boss->AttackPoint);
			const float Flight = FMath::Max(Boss->Tune(TEXT("boulderMinFlight"), 0.9f), Dist / Boss->Tune(TEXT("boulderSpeed"), 1400.f));
			SpawnLob(Boss, TEXT("stump"), From, Boss->AttackPoint, Flight, Dist * 0.22f + 300.f, 0.f, Attack.Damage, Attack.Size, 0.f, 5);
			++St.StumpsInFlight;
			Boss->bThrown = true;
			return true;
		}
		if (Id == TEXT("regrow_bark"))
		{
			// It held still the full 1.5 s: the bark is whole again on that spot.
			const int32 P = St.RegrowPlate;
			if (P >= 0 && St.PlateHP[P] <= 0.f && St.BurnUntil[P] <= T)
			{
				St.PlateHP[P] = Boss->Tune(TEXT("barkPlateHealth"), 400.f);
				St.StrippedAt[P] = 0.0;
				WRFx::Broadcast(Boss, TEXT("gnarl.bark_regrow"), FbPlatePoint(Boss, P), Boss->GetActorRotation(), 1.2f, TEXT("forest.bark_regrow"));
				UE_LOG(LogWildreach, Log, TEXT("BOSS Old Gnarl regrows plate %d"), P);
			}
			St.RegrowPlate = -1;
			if (Body) Body->Pose = 0;
			return true;
		}
		if (Id == TEXT("root_slam"))
		{
			WRFx::Broadcast(Boss, TEXT("gnarl.soil_pour"), FbFeet(Boss), Boss->GetActorRotation(), R / 100.f, TEXT("stomp"));
		}
		return false;
	}
	if (K == TEXT("bramblequeen"))
	{
		if (Id == TEXT("sting_lance"))
		{
			// A forward lunge 6 m: she runs it herself (in flight, the body's own sweep stops it on paper or wood).
			Boss->StrikeFrom = Boss->GetActorLocation();
			FVector Aim = (Boss->AttackPoint + FVector(0.f, 0.f, 90.f) - Boss->StrikeFrom).GetSafeNormal();
			if (Aim.IsNearlyZero()) Aim = Fwd;
			Boss->StrikeTo = Boss->StrikeFrom + Aim * Boss->Tune(TEXT("stingLunge"), 600.f);
			WRSound::PlayAt(Boss, TEXT("forest.wasp_hum"), Boss->GetActorLocation(), 1.2f);
			return true;
		}
		if (Id == TEXT("pheromone_mark"))
		{
			// The pale dust: everyone in the 6 m, 60 deg cone in front of her is marked 30 s (all her wasps go for them).
			const float Reach = Attack.Range, Half = Attack.Size;
			for (AWRCharacterBase* Who : FbFoes(Boss, Reach + R, Boss->GetActorLocation()))
			{
				const float B = FMath::FindDeltaAngleDegrees(Boss->AttackFacing.Yaw, (Who->GetActorLocation() - Boss->GetActorLocation()).Rotation().Yaw);
				if (FMath::Abs(B) <= Half + 6.f) WRForest::MarkForWasps(Who, Boss->Tune(TEXT("markSeconds"), 30.f));
			}
			WRFx::Broadcast(Boss, TEXT("queen.pheromone_cloud"), Boss->GetActorLocation() + Fwd * (Reach * 0.5f), Boss->AttackFacing, Reach / 600.f, TEXT("forest.scout_mark"));
			return true;
		}
		if (Id == TEXT("swarm_call"))
		{
			// Six Bramblewasps out of the comb she bowed over (only while it lives).
			const AWRForestProp* C = St.Combs.IsValidIndex(St.SwarmComb) ? St.Combs[St.SwarmComb].Get() : nullptr;
			St.NextSwarmAt = T + Boss->Tune(TEXT("swarmEvery"), 25.f);
			if (!C || C->State != 0)
			{
				return true;
			}
			AWRCharacterBase* Marked = nullptr;
			for (AWRCharacterBase* Who : FbFoes(Boss, Boss->GetChaseRadius(), Boss->GetHome())) if (WRForest::HasWaspMark(Who)) { Marked = Who; break; }
			const int32 N = FMath::RoundToInt(Boss->Tune(TEXT("swarmSize"), 6.f));
			for (int32 i = 0; i < N; ++i)
			{
				const FVector At = C->HitPoint() + FRotator(0.f, 360.f * i / N, 0.f).Vector() * 120.f;
				if (AWRCreature* W = FbSpawnRow(World, TEXT("bramblewasp"), At, FRotator::ZeroRotator))
				{
					W->SetHome(St.Home);
					W->SetAggroTarget(Marked ? Marked : Target);
					St.Wasps.Add(W);
				}
			}
			WRFx::Broadcast(Boss, TEXT("queen.swarm_out"), C->HitPoint(), FRotator::ZeroRotator, 1.f, TEXT("forest.wasp_swarm"));
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Bramblequeen calls %d wasps from comb %d"), N, C->Index);
			return true;
		}
		return false;
	}
	if (K == TEXT("grey_matron"))
	{
		if (Id == TEXT("canopy_drop"))
		{
			// Off the planks onto where you stood (the shadow was there since the tell).
			St.bCanopy = false;
			St.Walk = -1;
			Boss->StrikeFrom = Boss->GetActorLocation();
			Boss->StrikeTo = Boss->AttackPoint + FVector(0.f, 0.f, HH + 10.f);
			const float Fall = FMath::Clamp(FMath::Sqrt(2.f * FMath::Max(100.f, Boss->StrikeFrom.Z - Boss->StrikeTo.Z) / 980.f) * 0.7f, 0.3f, 1.4f);
			Boss->StrikeEnds = T + Fall;
			UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
			Move->SetMovementMode(MOVE_Flying);
			Move->MaxFlySpeed = 0.f;
			WRSound::PlayAt(Boss, TEXT("forest.canopy_rustle"), Boss->GetActorLocation(), 1.3f);
			return true;
		}
		if (Id == TEXT("bridge_leap"))
		{
			if (St.LeapTo < 0)
			{
				Boss->StrikeEnds = T;
				return true;
			}
			// Across the arena tree to tree in a swooping bound that dips over the moss mid-way (raking whoever's under).
			Boss->StrikeFrom = St.LeapFrom + FVector(0.f, 0.f, HH + 4.f);
			Boss->StrikeTo = St.LeapEnd + FVector(0.f, 0.f, HH + 4.f);
			Boss->StrikeEnds = T + FMath::Clamp(FVector::Dist(Boss->StrikeFrom, Boss->StrikeTo) / Boss->Tune(TEXT("leapSpeed"), 2600.f), 0.6f, 2.4f);
			St.Walk = -1;
			St.bCanopy = false;   // in the air until she lands on the far platform (EndStrike)
			if (Body) Body->Pose = 0;
			WRSound::PlayAt(Boss, TEXT("forest.canopy_rustle"), Boss->GetActorLocation(), 1.2f);
			return true;
		}
		if (Id == TEXT("kit_scream"))
		{
			// Two Thornlynx out of the ferns at the ring's edge; they go for the furthest tame first.
			AWRCreature* Tame = FbFurthestTame(Boss);
			for (int32 i = 0; i < FMath::RoundToInt(Boss->Tune(TEXT("kits"), 2.f)); ++i)
			{
				const FVector Edge = St.Home + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * St.ArenaR * 0.85f;
				FVector G;
				if (!FbGround(World, Edge, G, 20000.f, 40000.f)) continue;
				if (AWRCreature* Kit = FbSpawnRow(World, TEXT("thornlynx"), G + FVector(0.f, 0.f, 120.f), (St.Home - G).Rotation()))
				{
					Kit->SetHome(St.Home);
					Kit->SetAggroTarget(Tame ? static_cast<AActor*>(Tame) : static_cast<AActor*>(Target));
					St.Kits.Add(Kit);
					WRFx::Broadcast(Boss, TEXT("matron.ferns_part"), G, FRotator::ZeroRotator, 1.f, TEXT("forest.canopy_rustle"));
				}
			}
			return true;
		}
		return false;
	}
	if (K == TEXT("hornwarden"))
	{
		if (Id == TEXT("bellow"))
		{
			// The herd charges the target together.
			for (const TWeakObjectPtr<AWRCreature>& H : St.Herd)
			{
				if (!H.IsValid() || !H->IsAlive()) continue;
				H->SetAsleep(false);
				H->SetAggroTarget(Target);
				H->SpeedBoost = Boss->Tune(TEXT("herdChargeSpeed"), 1.5f);
			}
			WRFx::BroadcastOn(Boss, TEXT("warden.bellow_breath"), TEXT("head"), 1.5f, TEXT("forest.hunt_horn"));
			if (Body) Body->Pose = 0;
			return true;
		}
		return false;
	}
	// Sylvaroth.
	if (Id == TEXT("uprooting"))
	{
		// It tears its lower body free: all four lines go dark 10 s (a free knot window); the ring hurts as it bursts.
		FbSetRooted(Boss, St, false, 0.f);
		for (const TWeakObjectPtr<AWRForestRootLine>& L : St.Lines) if (L.IsValid()) L->Darken(Boss->Tune(TEXT("uprootDark"), 10.f), 3);
		WRFx::Broadcast(Boss, TEXT("sylvaroth.uproot_burst"), FbFeet(Boss), Boss->GetActorRotation(), Attack.Size / 1000.f, TEXT("forest.uproot"));
		return false;   // the wave (the attack machine's ring) does the 60 and the knockdown
	}
	if (Id == TEXT("root_eruption"))
	{
		return true;   // TickStrike runs the bulge's front along its line
	}
	if (Id == TEXT("seed_bombs"))
	{
		// Six pods at where everyone stood at the launch, ~1.2 s in the air on a gravity arc, never inside 4 m.
		TArray<AWRCharacterBase*> Foes = FbFoes(Boss, Attack.Range, Boss->GetActorLocation());
		if (Foes.Num() == 0 && Target) Foes.Add(Target);
		const int32 N = FMath::RoundToInt(Boss->Tune(TEXT("seedPods"), 6.f));
		const FVector From = Boss->GetActorLocation() - Boss->GetActorForwardVector() * R * 0.3f + FVector(0.f, 0.f, HH * 0.9f);
		const float MinR = Boss->Tune(TEXT("podMin"), 400.f);
		for (int32 i = 0; i < N && Foes.Num() > 0; ++i)
		{
			AWRCharacterBase* Who = Foes[i % Foes.Num()];
			FVector To = FbFeet(Who);
			if (i >= Foes.Num()) To += FRotator(0.f, 360.f * i / N + 40.f, 0.f).Vector() * Boss->Tune(TEXT("podSpread"), 350.f);
			FVector Off = To - FbFeet(Boss);
			Off.Z = 0.f;
			if (Off.Size() < MinR + R) To = FbFeet(Boss) + (Off.IsNearlyZero() ? Fwd : Off.GetSafeNormal()) * (MinR + R);
			FVector G;
			FbGround(World, To, G);
			SpawnLob(Boss, TEXT("pod"), From, G, Boss->Tune(TEXT("podFlight"), 1.2f) + 0.06f * i, Boss->Tune(TEXT("podArc"), 700.f), 0.f, Attack.Damage, Boss->Tune(TEXT("podRadius"), 300.f), Boss->Tune(TEXT("podEntangle"), 0.8f), 3);
			++St.PodsInFlight;
		}
		if (Body) Body->Pose = 0;
		Boss->bThrown = true;
		return true;
	}
	if (Id == TEXT("root_hand"))
	{
		// The catch: whoever is still in the ring is lifted out of the ground.
		AWRForestProp* Hand = St.HandProp.Get();
		if (Body) Body->Pose = 0;
		if (!Hand)
		{
			return true;
		}
		AWRCharacterBase* Caught = nullptr;
		for (AWRCharacterBase* Who : FbFoes(Boss, Hand->Size + 60.f, Hand->GetActorLocation()))
		{
			if (FMath::Abs(FbFeet(Who).Z - Hand->GetActorLocation().Z) < 250.f && !Who->HasTag(WRTags::State_IFrames)) { Caught = Who; break; }
		}
		if (Caught)
		{
			Hand->SetState(1);
			Grab(Boss, St, Caught, TEXT("root_hand"));
		}
		else
		{
			Hand->SetState(3);
			WRFx::Broadcast(Boss, TEXT("sylvaroth.root_erupt"), Hand->GetActorLocation(), FRotator::ZeroRotator, 0.8f, TEXT("forest.uproot"));
		}
		return true;
	}
	if (Id == TEXT("canopy_shake"))
	{
		// Eight dead branches fall across the clearing over 4 s, each with its shadow a second before it lands; none
		// within 8 m of a heart-tree (they shelter you).
		const int32 N = FMath::RoundToInt(Boss->Tune(TEXT("shakeBranches"), 8.f));
		TArray<AWRCharacterBase*> Foes = FbFoes(Boss, St.ArenaR, St.Home);
		const float Shelter = Boss->Tune(TEXT("shakeShelter"), 800.f) + FbHeartRadius();
		for (int32 i = 0; i < N; ++i)
		{
			FVector Spot = St.Home;
			for (int32 Try = 0; Try < 12; ++Try)
			{
				Spot = (i % 2 == 0 && Foes.Num() > 0) && Try < 4 ? FbFeet(Foes[(i / 2) % Foes.Num()]) + FVector(FMath::RandPointInCircle(250.f), 0.f)
					: St.Home + FVector(FMath::RandPointInCircle(St.ArenaR * 0.85f), 0.f);
				bool bSheltered = false;
				for (const FVector& H : St.HeartAt) bSheltered |= FVector::Dist2D(H, Spot) < Shelter;
				if (!bSheltered) break;
			}
			FVector G;
			FbGround(World, Spot, G);
			SpawnLob(Boss, TEXT("branch"), G + FVector(0.f, 0.f, Boss->Tune(TEXT("branchHeight"), 3200.f)), G, Boss->Tune(TEXT("branchFall"), 0.8f), 0.f,
				Boss->Tune(TEXT("branchGap"), 0.5f) * i + 1.f, Attack.Damage, Boss->Tune(TEXT("branchRadius"), 260.f), 0.f, 4);
		}
		return true;
	}
	if (Id == TEXT("thorn_whip"))
	{
		// The vine snaps out along its line to the tame it uncoiled toward: 120 and pulled 15 m in.
		AWRCharacterBase* W = St.WhipAt.Get();
		if (W && W->IsAlive() && FVector::Dist2D(FbFeet(W), Boss->AttackPoint) < Boss->Tune(TEXT("whipCatch"), 450.f) + W->GetSimpleCollisionRadius())
		{
			FbHitWith(Boss, W, Attack.Damage, 120.f, 4, false, false, Boss->GetActorLocation(), TEXT("bleed"), 20.f);
			if (W->IsAlive() && !W->IsA<AWRPlantSummon>())
			{
				const FVector Pull = (Boss->GetActorLocation() - W->GetActorLocation()).GetSafeNormal2D();
				W->LaunchCharacter(Pull * Boss->Tune(TEXT("whipPull"), 1500.f) + FVector(0.f, 0.f, 350.f), true, true);
			}
		}
		WRFx::Broadcast(Boss, TEXT("sylvaroth.vine_snap"), Boss->AttackPoint, FRotator::ZeroRotator, 1.f, TEXT("nature.vine_lash"));
		return true;
	}
	if (Id == TEXT("thrash_spin"))
	{
		return true;   // TickStrike turns the arms round
	}
	if (Id == TEXT("re_root"))
	{
		// It kneels and drives both arms into the earth: every live line heals it for 6 s; it can't attack.
		St.bRerooting = true;
		FbSetRooted(Boss, St, true, Boss->Tune(TEXT("rerootSink"), 200.f));
		WRFx::Broadcast(Boss, TEXT("sylvaroth.reroot"), FbFeet(Boss), Boss->GetActorRotation(), R / 200.f, TEXT("forest.root_pulse"), Attack.Row.Active);
		return true;
	}
	return false;
}

bool FWRForestBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	FWRForestBossState& St = FbS(Boss);
	const double T = FbNow(Boss);
	const FName Id = Attack.Id;
	const float R = Boss->GetSimpleCollisionRadius();
	const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
	const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
	const auto Touch = [Boss, R](float Extra)
	{
		for (AWRCharacterBase* Who : FbFoes(Boss, R + Extra, Boss->GetActorLocation()))
		{
			if (FMath::Abs(Who->GetActorLocation().Z - Boss->GetActorLocation().Z) < Boss->GetSimpleCollisionHalfHeight() + Who->GetSimpleCollisionHalfHeight()) Boss->StrikeHit(Who);
		}
	};
	if (Id == TEXT("sting_lance"))
	{
		// The lunge, the stinger first: contact is the hit; paper or wood in the way stops her (and sticks it).
		FHitResult Stop;
		const FVector Want = FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A);
		Boss->SetActorLocation(Want, true, &Stop);
		Touch(Boss->Tune(TEXT("stingReach"), 90.f));
		if (Stop.bBlockingHit) Boss->StrikeEnds = T;
		return true;
	}
	if (Id == TEXT("canopy_drop"))
	{
		// Falling (faster as she goes), the blow lands when she does.
		Boss->SetActorLocation(FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A * A), false);
		if (A >= 0.85f)
		{
			for (AWRCharacterBase* Who : FbFoes(Boss, Attack.Size + R * 0.5f, Boss->StrikeTo)) Boss->StrikeHit(Who);
		}
		return true;
	}
	if (Id == TEXT("bridge_leap"))
	{
		// A bound that dips to just over the moss mid-way: whoever's under the line there is raked.
		FVector P = FMath::Lerp(Boss->StrikeFrom, Boss->StrikeTo, A);
		FVector G;
		FbGround(Boss->GetWorld(), P, G);
		const float Dip = FMath::Sin(A * PI);
		const float Low = G.Z + Boss->GetSimpleCollisionHalfHeight() + Boss->Tune(TEXT("leapDip"), 120.f);
		P.Z = FMath::Lerp(P.Z, FMath::Min(P.Z, Low), Dip);
		Boss->SetActorLocation(P, false);
		Boss->SetActorRotation(FRotator(0.f, (Boss->StrikeTo - Boss->StrikeFrom).Rotation().Yaw, 0.f));
		if (Dip > 0.6f) Touch(Boss->Tune(TEXT("leapRake"), 120.f));
		return true;
	}
	if (Id == TEXT("thrash_spin"))
	{
		// Both arms round, 360 deg, 12 m.
		const float Edge = -180.f + 360.f * A;
		for (AWRCharacterBase* Who : FbFoes(Boss, Attack.Size, Boss->GetActorLocation()))
		{
			const float B = FMath::FindDeltaAngleDegrees(Boss->AttackFacing.Yaw + 180.f, (Who->GetActorLocation() - Boss->GetActorLocation()).Rotation().Yaw);
			if (B <= Edge) Boss->StrikeHit(Who);
		}
		Boss->SetActorRotation(FRotator(0.f, Boss->AttackFacing.Yaw + 360.f * A, 0.f));
		return true;
	}
	if (Id == TEXT("root_eruption"))
	{
		// The roots burst up along the bulge from its end at the body outward (not a projectile: sidestep it).
		const float Front = A * Attack.Range;
		const float Half = Attack.Size;
		for (AWRCharacterBase* Who : FbFoes(Boss, Attack.Range + R + 400.f, Boss->GetActorLocation()))
		{
			const FVector To = Who->GetActorLocation() - St.EruptFrom;
			const float Along = FVector::DotProduct(To * FVector(1.f, 1.f, 0.f), St.EruptDir);
			const float Side = FMath::Abs(FVector::CrossProduct(St.EruptDir, To * FVector(1.f, 1.f, 0.f)).Z);
			if (Along >= -50.f && Along <= Front && Side <= Half + Who->GetSimpleCollisionRadius()) Boss->StrikeHit(Who);
		}
		static const int32 Bursts = 6;
		const int32 Was = FMath::FloorToInt(FMath::Clamp(float(T - DeltaSeconds - Boss->StrikeStarted) / Span, 0.f, 1.f) * Bursts);
		const int32 Reached = FMath::FloorToInt(A * Bursts);
		for (int32 b = Was; b < Reached && b < Bursts; ++b)
		{
			FVector G;
			FbGround(Boss->GetWorld(), St.EruptFrom + St.EruptDir * Attack.Range * (b + 0.5f) / Bursts, G);
			WRFx::Broadcast(Boss, TEXT("sylvaroth.root_erupt"), G, St.EruptDir.Rotation(), 1.2f, b % 2 == 0 ? FName(TEXT("forest.uproot")) : NAME_None);
		}
		return true;
	}
	if (Id == TEXT("knuckle_charge"))
	{
		// It never hits its own heart-trees: it stops 6 m short of one (and stands there, open).
		const FVector Dir = Boss->AttackFacing.Vector().GetSafeNormal2D();
		const float Stop = Boss->Tune(TEXT("chargeHeartStop"), 600.f) + FbHeartRadius() + R;
		for (const FVector& H : St.HeartAt)
		{
			const FVector To = (H - Boss->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			if (To.Size() < Stop && FVector::DotProduct(To.GetSafeNormal(), Dir) > 0.3f)
			{
				Boss->StrikeEnds = T;
				Boss->GetCharacterMovement()->StopMovementImmediately();
				WRFx::Broadcast(Boss, TEXT("land.dust"), FbFeet(Boss), Boss->GetActorRotation(), R / 100.f, TEXT("stomp"));
				break;
			}
		}
		return false;
	}
	if (Id == TEXT("antler_charge") && WRForest::IsEntangled(Boss))
	{
		Boss->StrikeEnds = T;   // roots caught his legs mid-charge
		return false;
	}
	return FbOwnStrike(Id);
}

void FWRForestBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	const double T = FbNow(Boss);
	const FName Id = Attack.Id;
	if (Id == TEXT("sting_lance"))
	{
		// A miss: the stinger sticks in paper or wood (1.1 s she wrenches at it).
		if (Boss->StrikeHits.Num() == 0)
		{
			St.StuckUntil = T + Boss->Tune(TEXT("stuckSeconds"), 1.1f);
			if (Body) Body->bStuck = true;
			WRFx::Broadcast(Boss, TEXT("queen.sting_stuck"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius(), Boss->GetActorRotation(), 1.f, TEXT("forest.comb_break"));
		}
	}
	else if (Id == TEXT("canopy_drop"))
	{
		// Down on the moss: the punish, then back up her nearest oak (unless she's down for good).
		Boss->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		St.LandedAt = T;
		WRFx::Broadcast(Boss, TEXT("land.dust"), FbFeet(Boss), FRotator::ZeroRotator, 1.4f, TEXT("land"));
	}
	else if (Id == TEXT("bridge_leap"))
	{
		if (St.LeapTo >= 0)
		{
			St.Tree = St.LeapTo;
			St.bCanopy = true;
		}
		St.LeapTo = -1;
		St.Walk = -1;
	}
	else if (Id == TEXT("re_root"))
	{
		St.bRerooting = false;
		FbSetRooted(Boss, St, false, 0.f);
	}
	else if (Id == TEXT("hammer_fists"))
	{
		// The low shockwave ring after the fists land.
		FWRForestBossWave Wave;
		Wave.At = FbFeet(Boss) + Boss->AttackFacing.Vector().GetSafeNormal2D() * Boss->GetSimpleCollisionRadius();
		Wave.Start = T;
		Wave.MaxR = Boss->Tune(TEXT("hammerWave"), 800.f);
		Wave.Damage = Boss->Tune(TEXT("hammerWaveDamage"), 40.f);
		Wave.Speed = WRTune(TEXT("bosses.waveSpeed"), 1200.f);
		Wave.bLow = true;
		St.Waves.Add(MoveTemp(Wave));
		WRFx::Broadcast(Boss, TEXT("shockwave"), FbFeet(Boss), FRotator::ZeroRotator, Boss->Tune(TEXT("hammerWave"), 800.f) / 100.f, TEXT("stomp"));
	}
	if (Body && Id != TEXT("sting_lance")) Body->Pose = 0;
}

void FWRForestBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return;
	}
	const FWRForestBossState* St = FbFind(Boss);
	if (!St)
	{
		return;
	}
	if (St->Kind == TEXT("hornwarden") && FbNow(Boss) < St->EnragedUntil)
	{
		Hit.Damage *= FbBossTune(Boss, TEXT("enrageDamage"), 1.3f);
	}
	if (St->Kind == TEXT("sylvaroth") && Boss->Phase >= 2 && FbIsRootedMove(Attack.Id))
	{
		Hit.Damage *= FbBossTune(Boss, TEXT("p2DamageScale"), 1.1f);
	}
}

void FWRForestBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || !Victim || !Victim->IsAlive() || Result != EWRHitResult::Hit)
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	if (AttackId == TEXT("grasp"))
	{
		Grab(Boss, St, Victim, TEXT("grasp"));
	}
	else if (AttackId == TEXT("antler_toss"))
	{
		// Scooped on the tines and thrown 10 m.
		const float K = Boss->Tune(TEXT("tossThrow"), 1000.f);
		Victim->LaunchCharacter((Victim->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D() * K + FVector(0.f, 0.f, K * 0.55f), true, true);
	}
}

float FWRForestBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return Damage;
	}
	FWRForestBossState& St = FbS(Boss);
	const double T = FbNow(Boss);
	const FName K = St.Kind;
	if (K == TEXT("old_gnarl") && !Hit.bDamageOverTime)
	{
		// Strip and burn: the blow lands on a plate. Bark: axes x1.5, into the plate's 400. Pale wood: fire x2, and it burns.
		const int32 P = FbPlateFor(Boss, FbHitFrom(Boss, Hit));
		const bool bFire = FbIsFire(Hit);
		const bool bAxe = Hit.bMelee && FbIsAxe(Hit.Instigator.Get());
		if (St.PlateHP[P] > 0.f)
		{
			Damage *= bAxe ? Boss->Tune(TEXT("axeOnBark"), 1.5f) : 1.f;
			St.PlateHP[P] -= Damage;
			if (St.PlateHP[P] <= 0.f)
			{
				St.PlateHP[P] = 0.f;
				St.StrippedAt[P] = T;
				WRFx::Broadcast(Boss, TEXT("gnarl.bark_strip"), FbPlatePoint(Boss, P), Boss->GetActorRotation(), 1.5f, TEXT("forest.bark_strip"));
				UE_LOG(LogWildreach, Log, TEXT("BOSS Old Gnarl's plate %d stripped by %s"), P, *GetNameSafe(Hit.Instigator.Get()));
			}
			else
			{
				WRFx::Broadcast(Boss, TEXT("gnarl.bark_chips"), FbPlatePoint(Boss, P), Boss->GetActorRotation(), bAxe ? 1.f : 0.6f, NAME_None);
			}
		}
		else if (bFire)
		{
			Damage *= Boss->Tune(TEXT("fireOnPale"), 2.f);
			St.BurnUntil[P] = FMath::Max(St.BurnUntil[P], T + Boss->Tune(TEXT("plateBurnSeconds"), 8.f));
		}
		// Fire during the regrow cancels it: it staggers 2 s.
		if (bFire && Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id == TEXT("regrow_bark") && Boss->StrikeState == AWRBoss::EStrike::Windup)
		{
			St.RegrowPlate = -1;
			CancelAttack(Boss);
			Boss->Status->Stagger(Boss->Tune(TEXT("regrowCancelStagger"), 2.f));
			WRFx::Broadcast(Boss, TEXT("gnarl.reel"), Boss->GetActorLocation(), Boss->GetActorRotation(), 1.f, TEXT("forest.thorn_burn"));
		}
		return Damage;
	}
	if (K == TEXT("bramblequeen") && Hit.bDamageOverTime && Boss->HasTag(WRTags::Status_Poison) && !Boss->HasTag(WRTags::Status_Burn) && !Boss->HasTag(WRTags::Status_Rot))
	{
		return Damage * Boss->Tune(TEXT("poisonTaken"), 0.5f);   // her own venom's kin: poison x0.5
	}
	if (K == TEXT("hornwarden") && !Hit.bDamageOverTime)
	{
		// The antlers: 1,200 each; a blow near one goes into it (break one side and his toss is gone).
		const FWRCreatureDef* Def = Boss->GetDef();
		const FVector At = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
		if (Def && !At.IsZero())
		{
			for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
			{
				const FWRCreatureDef::FBodyPoint& Part = Def->Parts[i];
				const int32 Side = Part.Id == TEXT("antler_left") ? 0 : Part.Id == TEXT("antler_right") ? 1 : -1;
				if (Side < 0 || (Boss->PartsBroken & (1 << i))) continue;
				if (FVector::Dist(Boss->BodyPointLocation(Part), At) > WRTune(TEXT("stance.partRadius"), 120.f) + Boss->GetSimpleCollisionRadius() * 0.35f) continue;
				St.AntlerHP[Side] -= Damage;
				if (St.AntlerHP[Side] <= 0.f)
				{
					Boss->PartsBroken |= uint8(1 << i);
					Boss->OnRep_PartsBroken();
					Boss->OnPartBroken(Part, Hit.Instigator.Get());
				}
				break;
			}
		}
		return Damage;
	}
	if (K == TEXT("sylvaroth"))
	{
		// Weak to Necromancy (its decay): x1.5. Fire is neutral on its wet living sap.
		if (Hit.Buildups.FindRef(TEXT("decay")) > 0.f) Damage *= WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f);
	}
	return Damage;
}

bool FWRForestBoss::OwnWeakness(const AWRBoss* Boss)
{
	// None of them is weak to fire or frost by the island's rule: Old Gnarl's fire is its pale wood, Sylvaroth's weakness
	// is Necromancy.
	return IsForestBoss(Boss);
}

void FWRForestBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || Damage <= 0.f)
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	if (St.Kind == TEXT("old_gnarl") && !Hit.bDamageOverTime && FbIsFire(Hit))
	{
		// Pale wood takes the fire's burn buildup twice.
		const int32 P = FbPlateFor(Boss, FbHitFrom(Boss, Hit));
		const float Burn = Hit.BurnBuildup + Hit.Buildups.FindRef(TEXT("burn"));
		if (St.PlateHP[P] <= 0.f && Burn > 0.f && Boss->IsAlive())
		{
			Boss->Status->AddBuildup(WRTags::Element_Fire, Burn * (Boss->Tune(TEXT("burnOnPale"), 2.f) - 1.f), Cast<AWRCharacterBase>(Hit.Instigator.Get()));
		}
	}
	// An ally's blows on the arm that holds a friend (Old Gnarl's grasp: 100 frees them).
	if (St.Hold.Who.IsValid() && St.Hold.How == TEXT("grasp") && Hit.Instigator.Get() != St.Hold.Who.Get() && FbSurvivorSide(Hit.Instigator.Get()))
	{
		const FVector From = FbHitFrom(Boss, Hit);
		if (Hit.ImpactPoint.IsZero() || FbPlateFor(Boss, From) != 2) St.Hold.ArmDamage += Damage;
	}
}

void FWRForestBoss::PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return;
	}
	if (Part.OnBreak == TEXT("antler"))
	{
		WRFx::Broadcast(Boss, TEXT("warden.antler_break"), Boss->BodyPointLocation(Part), Boss->GetActorRotation(), 2.f, TEXT("part.break"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden's %s breaks"), *Part.Name);
	}
	else if (Part.OnBreak == TEXT("arm_collapse"))
	{
		// That side collapses for 3 s: a crit window.
		CancelAttack(Boss);
		UWRCombatLibrary::BreakStance(Boss, Boss->Tune(TEXT("armCollapse"), 3.f), By);
		WRFx::Broadcast(Boss, TEXT("sylvaroth.arm_collapse"), Boss->BodyPointLocation(Part), Boss->GetActorRotation(), 2.f, TEXT("forest.knot_break"));
	}
}

void FWRForestBoss::WallStun(AWRBoss* Boss, float Seconds)
{
	if (!IsForestBoss(Boss))
	{
		return;
	}
	// The Hornwarden into a trunk or a stone: stunned (the attack machine breaks his stance for the seconds).
	WRFx::Broadcast(Boss, TEXT("warden.antler_crack"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius(), Boss->GetActorRotation(), 1.5f, TEXT("part.break"));
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s stunned %.1f s against the wood"), *Boss->GetDisplayLabel(), Seconds);
}

void FWRForestBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss) || Boss->CreatureId != TEXT("sylvaroth"))
	{
		return;
	}
	FWRForestBossState& St = FbS(Boss);
	UWRForestBossBody* Body = UWRForestBossBody::Get(Boss);
	const double T = FbNow(Boss);
	if (NewPhase == 1)
	{
		for (const TWeakObjectPtr<AWRForestRootLine>& L : St.Lines) if (L.IsValid()) L->bHeartOnly = false;
		for (const TWeakObjectPtr<AWRForestProp>& H : St.Hearts) if (H.IsValid()) H->SetState(0);
		if (Body) { Body->Sectors.Init(0, 6); Body->SectorUntil.Init(0.f, 6); }
		return;
	}
	if (NewPhase == 2)
	{
		// The walking root: out of the earth, the vines come every 30 s.
		if (St.bRooted) FbSetRooted(Boss, St, false, 0.f);
		St.NextSectorsAt = T + Boss->Tune(TEXT("firstVines"), 8.f);
		return;
	}
	// Hidden phase 3 (artifacts): every heart-tree wakes; each line now dies only with its amber heart.
	for (const TWeakObjectPtr<AWRForestProp>& H : St.Hearts)
	{
		if (!H.IsValid()) continue;
		H->Health = H->MaxHealth = Boss->Tune(TEXT("p3HeartHealth"), 1500.f);
		H->SetState(1);
	}
	for (const TWeakObjectPtr<AWRForestRootLine>& L : St.Lines) if (L.IsValid()) L->bHeartOnly = true;
	St.NextShakeAt = T + Boss->Tune(TEXT("p3ShakeEvery"), 20.f);
	for (int32 i = 0; i < 4; ++i) St.NextHeartSweep[i] = 0.0;
	WRFx::Broadcast(Boss, TEXT("sylvaroth.hearts_wake"), St.Home, FRotator::ZeroRotator, St.ArenaR / 1000.f, TEXT("forest.root_pulse"), 3.f);
	UE_LOG(LogWildreach, Log, TEXT("BOSS Sylvaroth: the heart-trees wake"));
}

bool FWRForestBoss::PlaceRunback(AWRBoss* Boss)
{
	using namespace WRForestBossImpl;
	if (!IsForestBoss(Boss))
	{
		return false;
	}
	if (Boss->CreatureId == TEXT("hornwarden"))
	{
		return true;   // roaming: no shrine-stone (as the island's roamers)
	}
	const FWRForestData::FArena* Row = FWRForestData::Get().Arenas.Find(Boss->CreatureId);
	if (!Row || Row->Runback.IsNearlyZero())
	{
		return false;
	}
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
	{
		if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	}
	FVector G;
	if (!FbGround(Boss->GetWorld(), Row->Runback, G, 20000.f, 40000.f))
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

FName FWRForestBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == TEXT("sylvaroth"))
	{
		// The Verdant core (healing potions also heal your tames nearby); a Scarred one when the game has its row.
		return bScarred && FWRItemRegistry::Get().Find(TEXT("verdant_core_scarred")) ? FName(TEXT("verdant_core_scarred")) : FName(TEXT("verdant_core"));
	}
	return Fallback;
}

float FWRForestBoss::StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance)
{
	const AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!Boss || Boss->CreatureId != TEXT("hornwarden"))
	{
		return 1.f;
	}
	// Held by roots, his stance takes x2.
	(void)Hit;
	(void)Stance;
	return WRForest::IsEntangled(Boss) ? WRTune(TEXT("bosses.hornwarden.heldStance"), 2.f) : 1.f;
}

void FWRForestBoss::AnyDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	using namespace WRForestBossImpl;
	if (!Target || Damage <= 0.f || !Cast<AWRCreature>(Target) || !FbSurvivorSide(Hit.Instigator.Get()))
	{
		return;
	}
	// Never hurt his herd: a blow on any of them from you or your tames enrages him 60 s.
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRForestBossState>& P : FbStates())
	{
		if (P.Value.Kind != TEXT("hornwarden") || !P.Key.IsValid()) continue;
		bool bHerd = false;
		for (const TWeakObjectPtr<AWRCreature>& H : P.Value.Herd) bHerd |= H.Get() == Target;
		if (!bHerd) continue;
		AWRBoss* Boss = const_cast<AWRBoss*>(P.Key.Get());
		const double T = FbNow(Boss);
		const bool bWas = T < P.Value.EnragedUntil;
		P.Value.EnragedUntil = T + Boss->Tune(TEXT("enrageSeconds"), 60.f);
		if (!bWas)
		{
			P.Value.NextBellowAt = T + 1.0;
			WRFx::BroadcastOn(Boss, TEXT("warden.enrage"), TEXT("head"), 1.5f, TEXT("forest.hunt_horn"));
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Hornwarden enrages: his herd was hurt by %s"), *GetNameSafe(Hit.Instigator.Get()));
		}
		if (AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get()); By && !Boss->bEngaged && Boss->IsHostileTo(By))
		{
			Boss->NotifyAttackedBy(By);
		}
	}
}

bool FWRForestBoss::HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire)
{
	using namespace WRForestBossImpl;
	if (!Thing || !Thing->HasAuthority())
	{
		return false;
	}
	const bool bAxe = FbIsAxe(By);
	if (AWRForestWalkway* Wk = Cast<AWRForestWalkway>(Thing))
	{
		const int32 Anchor = Wk->AnchorOf(Comp);
		if (Anchor < 0)
		{
			return false;
		}
		// A rope anchor: a blade cuts it (an axe best), fire burns through it.
		return Wk->TakeHit(Anchor, By, Damage * (bAxe ? WRTune(TEXT("bosses.grey_matron.axeOnRope"), 1.5f) : 1.f) * (bFire ? WRTune(TEXT("bosses.grey_matron.fireOnRope"), 1.5f) : 1.f));
	}
	if (AWRForestRootLine* Line = Cast<AWRForestRootLine>(Thing))
	{
		return Line->TakeHit(By, Damage, bFire, bAxe);
	}
	if (AWRForestProp* Prop = Cast<AWRForestProp>(Thing))
	{
		return Prop->TakeHit(By, Damage, bFire, bAxe);
	}
	return false;
}

bool FWRForestBoss::BolaHold(AWRCharacterBase* Target)
{
	const AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!Boss || Boss->CreatureId != TEXT("hornwarden"))
	{
		return false;
	}
	// A bola round his legs holds him like a snare (the one mini roots and cords hold).
	WRForest::Entangle(Target, WRTune(TEXT("bosses.hornwarden.snareHold"), 3.f), nullptr, true);
	return true;
}

void FWRForestBoss::BlightLanded(UWorld* World, const FVector& At, AActor* Thrower)
{
	using namespace WRForestBossImpl;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// On a Root Knot: that line dies 90 s with no damage needed (the Armillaria tree-killer).
	const float Reach = WRTune(TEXT("bosses.forest.blightReach"), 400.f);
	for (TActorIterator<AWRForestRootLine> It(World); It; ++It)
	{
		if (FVector::Dist(FVector(It->KnotAt), At) < Reach)
		{
			It->Darken(WRTune(TEXT("bosses.sylvaroth.blightSeconds"), 90.f), 2);
			UE_LOG(LogWildreach, Log, TEXT("FOREST Bootlace blight on Root Knot %d (by %s)"), It->Index, *GetNameSafe(Thrower));
		}
	}
	// On a stripped spot of Old Gnarl: it can't regrow there for 30 s.
	for (TPair<TWeakObjectPtr<const AWRBoss>, FWRForestBossState>& P : FbStates())
	{
		const AWRBoss* Boss = P.Key.Get();
		if (!Boss || P.Value.Kind != TEXT("old_gnarl")) continue;
		for (int32 Plate = 0; Plate < 4; ++Plate)
		{
			if (P.Value.PlateHP[Plate] > 0.f || FVector::Dist(FbPlatePoint(Boss, Plate), At) > Reach + Boss->GetSimpleCollisionRadius() * 0.5f) continue;
			P.Value.NoRegrowUntil[Plate] = FbNow(Boss) + WRTune(TEXT("bosses.old_gnarl.blightNoRegrow"), 30.f);
			WRFx::Broadcast(World, TEXT("sylvaroth.blight"), FbPlatePoint(Boss, Plate), FRotator::ZeroRotator, 0.8f, NAME_None);
		}
	}
}

void FWRForestBoss::LobLanded(AWRBoss* Boss, FName Kind)
{
	using namespace WRForestBossImpl;
	FWRForestBossState* St = FbFind(Boss);
	if (!St)
	{
		return;
	}
	if (Kind == TEXT("pod")) St->PodsInFlight = FMath::Max(0, St->PodsInFlight - 1);
	if (Kind == TEXT("stump")) St->StumpsInFlight = FMath::Max(0, St->StumpsInFlight - 1);
}

void FWRForestBoss::SpawnLob(AWRBoss* Boss, FName Kind, const FVector& From, const FVector& To, float Flight, float Arc, float Delay, float Damage, float Radius, float Entangle, uint8 Impact)
{
	using namespace WRForestBossImpl;
	UWorld* World = Boss->GetWorld();
	const FTransform Where(FRotator::ZeroRotator, From);
	AWRForestLob* Lob = World->SpawnActorDeferred<AWRForestLob>(AWRForestLob::StaticClass(), Where, Boss, Boss, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Lob)
	{
		return;
	}
	FWRHit Hit;
	Hit.Instigator = Boss;
	Hit.Damage = Damage * FbDamageScale() * (Boss->Phase >= 3 ? Boss->Tune(TEXT("phase3DamageScale"), 1.4f) : 1.f);
	Hit.PoiseDamage = Impact >= 5 ? 150.f : Impact >= 4 ? 100.f : 50.f;
	Hit.Impact = Impact;
	Hit.bHeavy = true;
	Hit.bProjectile = true;
	Hit.Tier = EWRHitTier::Heavy;
	Hit.Element = WRTags::Element_Physical;
	Hit.PhysType = 2;
	Lob->Hit = Hit;
	Lob->Radius = Radius;
	Lob->EntangleSeconds = Entangle;
	Lob->Thrower = Boss;
	Lob->FinishSpawning(Where);
	Lob->Launch(Kind, From, To, Flight, Arc, Delay);
}
