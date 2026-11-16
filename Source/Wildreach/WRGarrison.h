#pragma once
// The NPC strongholds' garrisons (Aaron 11-16, q-stronghold-garrison: "make the NPC strongholds designed like elden ring
// areas, with small, medium, and boss enemies"; showcase-notes cooks/build-wild-strongholds.md 2.4-2.5).
//
// A soldier is a person on the hero's own body (MetaHuman rig, the survivor's real clips and armour looks), driven by a
// server brain that presses the same ability inputs a survivor does: so a footman's sword chain, a captain's guard and
// charged heavy and a knight's hyperarmoured greatsword are the survivor's real move sets from Data/weapons.json, and
// every blow goes through ApplyHit with the whole hit stack. Archers draw and loose the hunting bow (a real arrow with
// drop, aimed where you stand when it leaves: never homing). The warlord adds its own data rows (leaps, kicks, a thrown
// harpoon, bursts) and a phase change at half health that is itself an attack.
//
// Ranks and warlords are data: Data/strongholds.json "ranks" and "warlords" (first drafts, tuned in playtests).

#include "CoreMinimal.h"
#include "WRCharacter.h"
#include "WRGarrison.generated.h"

class AWRGroundMarker;
class AWRPlayerState;

/** A rank's numbers (strongholds.json ranks.<id>). */
struct FWRHoldRank
{
	FName Id;
	FString Title;           // "Reaver footman": the name over its head (the fort's faction word in front)
	FName Tier;              // small / medium / boss
	float Health = 160.f;
	float Stamina = 100.f;
	float Poise = 30.f;
	float Armor = 10.f;
	float Sight = 1800.f;    // cm
	float Cone = 120.f;      // degrees, whole cone
	float Hear = 1500.f;
	float Leash = 3000.f;
	float WalkSpeed = 160.f;
	float RunSpeed = 420.f;
	float Guard = 0.2f;      // chance to raise the guard against a swing in front
	float GuardShield = 0.5f;// the same with a shield on the arm
	float Dodge = 0.15f;
	float Heavy = 0.25f;
	float Charged = 0.1f;
	int32 ChainMax = 3;
	float KickAfter = 0.f;   // > 0: kicks a guard held this long (captains)
	float ShotDamage = 0.f;  // archers: the arrow's damage
	float DrawSeconds = 1.1f;
	float Reload = 1.4f;
	float KeepMin = 800.f, KeepMax = 3000.f;
	float HornSeconds = 2.f; // horn-blowers: the wind-up
	TArray<TPair<FName, int32>> Loot;
	float RareChance = 0.f;  // its weapon or chest piece
	FName RareItem;          // "weapon" / "chest" / an item id
};

/** One of a warlord's own moves (strongholds.json warlords.<id>.moves[]): CAL 1.13's row grammar. */
struct FWRHoldMove
{
	FName Id;
	FName Shape = TEXT("cone");   // cone, self, line, target (leap / slam where the target stood), throw (a projectile)
	float Range = 300.f;          // reach: cone / line length, leap and throw distance
	float Size = 60.f;            // half angle (cone), radius (self / target), half width (line)
	float Windup = 0.8f;
	float Hold = 0.f;             // held at the top: the final cue 0.2 s before contact
	float Active = 0.2f;
	float Punish = 1.2f;
	float Damage = 40.f;
	float Poise = 80.f;
	uint8 Impact = 3;
	FName Element;                // fire / frost / none
	float Buildup = 0.f;
	bool bUnblockable = false;
	bool bParryable = false;
	bool bLow = false;
	bool bGrab = false;
	bool bGuardBreak = false;     // a kick: breaks a held guard
	bool bTransition = false;     // plays at the phase change
	bool bTamePunish = false;     // picks the furthest tame first
	bool bLunge = false;          // line: it runs the line itself, striking what it passes
	int32 HoldOn = 1;             // which blow of a combo carries the hold (1 = the first)
	int32 Phase = 1;
	int32 MaxPhase = 9;
	float Cooldown = 6.f;
	float WClose = 1.f, WMid = 1.f, WFar = 0.f;   // < 400, < 1200, beyond
	FName Clip = TEXT("heavy");   // the hero clip it plays: heavy, light, kick, throw, cast
	float ProjectileSpeed = 1000.f;
	int32 Repeats = 1;
	float Gap = 0.6f;
	float HazardSeconds = 0.f;    // burning ground / ice left where it struck
	float HazardRadius = 200.f;
	FName Sound;
	FName Fx;
	FString Mesh;                 // throw: the projectile's mesh
	FString MeshAlong;
};

struct FWRWarlordDef
{
	FName Id;
	FString Name;
	FName Element;
	float Health = 2600.f;
	float Poise = 90.f;
	float Armor = 30.f;
	float PerFighter = 0.8f;      // +80% health per extra fighter (CAL 1.14)
	float Phase2At = 0.5f;
	float Phase2Speed = 1.12f;
	float Phase2Buildup = 0.f;    // phase 2: every blow of his weapon also builds his element
	float ResetSeconds = 8.f;
	FName FirstKill;
	TArray<TPair<FName, int32>> Loot;
	TArray<FWRHoldMove> Moves;
};

/** The ranks and warlords (read once; WR.ReloadTuning doesn't re-read). */
struct WILDREACH_API FWRGarrisonData
{
	static const FWRGarrisonData& Get();
	TMap<FName, FWRHoldRank> Ranks;
	TMap<FName, FWRWarlordDef> Warlords;
	const FWRHoldRank* Rank(FName Id) const { return Ranks.Find(Id); }
	const FWRWarlordDef* Warlord(FName Id) const { return Warlords.Find(Id); }
};

UENUM()
enum class EWRGarrisonState : uint8
{
	Post,     // at its post (or walking its route)
	Look,     // turned to something seen at the edge, about to commit
	Fight,
	Search,   // at the last place it saw its mark
	Return,   // walking back to its post
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRGarrison : public AWRCharacter
{
	GENERATED_BODY()

public:
	AWRGarrison(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetDisplayLabel() const override;
	virtual bool IsHostileTo(const AWRCharacterBase* Other) const override;
	virtual void NotifyAttackedBy(AActor* Attacker) override;
	virtual void OnDamageTaken(const FWRHit& Hit, float Damage) override;

	/** Server, before FinishSpawning: who it is (its fort row) and what it carries. */
	void Setup(FName InHold, const struct FWRHoldSoldier& Row, const FString& Faction);

	UPROPERTY(Replicated) FName HoldId;
	UPROPERTY(Replicated) FName RankId;
	UPROPERTY(Replicated) FString SoldierName;
	/** Its warlords row (strongholds.json warlords), None for the rank and file; replicated for the boss bar. */
	UPROPERTY(Replicated) FName WarlordId;
	/** The warlord's phase (1, 2); replicated for the boss bar. */
	UPROPERTY(ReplicatedUsing=OnRep_Phase) int32 Phase = 1;
	/** The warlord is in a fight (the boss bar shows within its range). */
	UPROPERTY(Replicated) bool bEngaged = false;
	UPROPERTY(Replicated) int32 Participants = 1;

	bool IsWarlord() const { return !WarlordId.IsNone(); }
	FName GetWarlordId() const { return WarlordId; }
	FName GetSoldierId() const { return SoldierId; }
	/** Server: the fort's alarm: come to Where and fight whoever is there. */
	void Alarm(const FVector& Where, AActor* Seen);
	/** Server: a survivor or tame it should fight (seen or reported). */
	void Engage(AActor* Target);
	AActor* GetTarget() const { return Target.Get(); }

	/** Server: who has dealt how much (player name -> damage; tames count for their owner): the warlord's pay. */
	TMap<FString, float> Ledger;

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnDeath(AActor* Killer) override;

private:
	UFUNCTION() void OnRep_Phase();

	// --- the row it was made from
	FName SoldierId;
	FVector Post = FVector::ZeroVector;
	float PostYaw = 0.f;
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	double RoutePauseUntil = 0.0;
	FName MeleeWeapon;
	FName BowItem;
	FName ChestPiece;
	TSet<TWeakObjectPtr<AActor>> LungeHits;
	bool bShield = false;

	// --- the brain (server)
	EWRGarrisonState State = EWRGarrisonState::Post;
	TWeakObjectPtr<AActor> Target;
	FVector LastSeenAt = FVector::ZeroVector;
	double LastSeenTime = -100.0;
	double StateSince = 0.0;
	double NextThink = 0.0;
	double NextAct = 0.0;
	double BlockUntil = 0.0;
	double BackOffUntil = 0.0;
	float StrafeSign = 1.f;
	double NextStrafeFlip = 0.0;
	int32 ChainLeft = 0;
	double ChainPressAt = 0.0;
	double ChargeReleaseAt = 0.0;
	double TargetGuardSince = -1.0;
	FVector LastPos = FVector::ZeroVector;
	double StuckSince = -1.0;
	double LostPostSince = -1.0;
	// archers
	double DrawStarted = -1.0;
	double NextShot = 0.0;
	bool bMeleeOut = false;
	// horn-blowers
	double HornStarted = -1.0;
	bool bHornBlown = false;
	// the warlord
	int32 CurrentMove = INDEX_NONE;
	double MoveStarted = 0.0;
	double MoveStrikeAt = 0.0;
	double MoveEndsAt = 0.0;
	int32 MoveRepeat = 0;
	bool bMoveStruck = false;
	bool bFinalCue = false;
	FVector MoveAim = FVector::ZeroVector;
	FVector LeapFrom = FVector::ZeroVector;
	TMap<FName, double> MoveReadyAt;
	FName LastMove;
	int32 SameInARow = 0;
	bool bPhaseTwoPending = false;
	double EmptySince = -1.0;
	double NextParticipantCheck = 0.0;
	TWeakObjectPtr<AWRGroundMarker> MoveMarker;
	float SoloHealth = 0.f;
	double NextSpecialRoll = 0.0;
	double NextEmberAt = 0.0;

	const FWRHoldRank* GetRank() const;
	const FWRWarlordDef* GetWarlord() const;
	double Now() const;
	void Think();
	AActor* LookForTarget(bool& bOutClear) const;
	bool CanSee(const AActor* Who, float Range, float ConeDeg) const;
	bool IsValidMark(const AActor* Who) const;
	void SetState(EWRGarrisonState S);
	void MoveToward(const FVector& At, float Speed, float StopAt);
	int32 NearestMark() const;
	FVector HomePoint() const;
	void FaceToward(const FVector& At, float DeltaSeconds, float Rate);
	void TickPost(float DeltaSeconds);
	void TickFight(float DeltaSeconds);
	void TickMelee(AActor* Mark, float Dist, float DeltaSeconds);
	void TickArcher(AActor* Mark, float Dist, float DeltaSeconds);
	void TickHorn(AActor* Mark, float DeltaSeconds);
	void Shoot(AActor* Mark);
	void SetBowOut(bool bBow);
	void Press(EWRAbilityInput Input);
	void Release(EWRAbilityInput Input);
	bool Busy() const;
	float ClassReach() const;
	// the warlord
	bool TickWarlord(AActor* Mark, float Dist, float DeltaSeconds);
	int32 PickMove(AActor* Mark, float Dist, AActor*& OutMark) const;
	void StartMove(int32 Index, AActor* Mark);
	void TickMove(float DeltaSeconds);
	void StrikeMove();
	bool InMoveShape(const FWRHoldMove& M, const FVector& Point, float Radius) const;
	void UpdateParticipants();
	void CheckPhase();
	void ResetWarlord();
	bool InWard(const FVector& At, float Margin) const;
	void Kick(AActor* Mark);
};
