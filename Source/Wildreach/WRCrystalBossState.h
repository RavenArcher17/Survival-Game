#pragma once

// The Crystal bosses' server-side fight state, shared by WRCrystalBosses.cpp and WRCrystalLyrathis.cpp (private).

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"

class AWRBoss;
class AWRCreature;
class AWRCharacterBase;
class AWRCrystalBossProp;
class AWRPrismBeam;
class AWRProjectile;

/** Server: one Crystal boss's fight. */
struct FWRCrystalBossState
{
	FName Kind;
	bool bBegun = false;
	FVector Home = FVector::ZeroVector;
	float ArenaR = 2000.f, Yaw = 0.f;

	// The Twins.
	TWeakObjectPtr<AWRCreature> Double;
	TWeakObjectPtr<AWRCrystalBossProp> Lens, Magazine;
	double NextSwapAt = 0.0, SwapStart = -100.0;
	FVector SwapA = FVector::ZeroVector, SwapB = FVector::ZeroVector;
	bool bDoubleGone = false;
	double WardUntil = 0.0;
	TSet<TWeakObjectPtr<AActor>> DoubleHits;
	FVector DoubleFrom = FVector::ZeroVector, DoubleTo = FVector::ZeroVector;
	bool bDoubleLine = false;

	// The Warden.
	struct FNoise { FVector At = FVector::ZeroVector; float Radius = 0.f; double When = 0.0; TWeakObjectPtr<AActor> By; FName Kind; };
	TArray<FNoise> Heard;
	FDelegateHandle Hearing;
	TArray<TWeakObjectPtr<AWRCrystalBossProp>> Chimes;
	TWeakObjectPtr<AWRCrystalBossProp> HammerOn;
	double HammerUntil = 0.0, NextBlowAt = 0.0, ListenStart = 0.0;
	FVector Goal = FVector::ZeroVector;
	bool bGoal = false, bBlastDue = false, bSnapDue = false;
	FVector SnapAt = FVector::ZeroVector;
	int32 RoundIndex = 0;

	// The Tyrant.
	float RowHP[6] = { 0, 0, 0, 0, 0, 0 };
	int32 RouteIndex = 0, RouteDir = 1;
	double FeedUntil = 0.0, RestUntil = 0.0;
	TArray<TWeakObjectPtr<AWRProjectile>> Shards;

	// Lyrathis (WRCrystalLyrathis.cpp).
	TArray<TWeakObjectPtr<AWRCrystalBossProp>> Turntables;
	TWeakObjectPtr<AWRPrismBeam> Beam, Beam2;
	float DomeHP = 0.f;
	double DomeUntil = 0.0, DomeRaisedAt = 0.0, NextBeamDue = 0.0;
	bool bDome = false, bDomePermanent = false, bWard85 = false, bWard65 = false;
	TArray<TWeakObjectPtr<AWRCreature>> Copies;
	TArray<double> CopyBack;
	TArray<double> FacedAt;
	double FlareStart = 0.0, StaggerUntil = 0.0;
	/** The Sovereign's grasp: who she holds, until when, the next squeeze, what allies have dealt her wrist. */
	TWeakObjectPtr<AWRCharacterBase> Held;
	double HeldUntil = 0.0, HeldTickAt = 0.0;
	float HeldFree = 0.f;
	/** Phase 3: the next cycle of her permanent dome after a break's stagger. */
	double P3NextDomeAt = 0.0;
};

/** The state of a Crystal boss's fight (made on first use). */
FWRCrystalBossState& WRCrystalBossStateOf(const AWRBoss* Boss);
FWRCrystalBossState* WRCrystalBossFind(const AWRBoss* Boss);
