#pragma once

#include "CoreMinimal.h"
#include "WRCreature.h"
#include "WRBoss.generated.h"

class UStaticMeshComponent;

/** Flat ground marker: attack telegraphs, footprints. Replicated so every player sees it. */
UCLASS()
class WILDREACH_API AWRGroundMarker : public AActor
{
	GENERATED_BODY()

public:
	AWRGroundMarker();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: spawn a marker. bBox = rectangle (Size = length, width), else a disc (Size.X = radius). */
	static AWRGroundMarker* Spawn(UWorld* World, const FVector& Location, const FRotator& Rotation, const FVector2D& Size, bool bBox, const FLinearColor& Color, float Seconds);

	UPROPERTY(ReplicatedUsing=OnRep_Shape) FVector2D Size = FVector2D(100.f, 100.f);
	UPROPERTY(ReplicatedUsing=OnRep_Shape) bool bBox = false;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) FLinearColor Color = FLinearColor::Red;

private:
	UFUNCTION() void OnRep_Shape();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Box;
};

/** Lingering ground hazard: lava cracks, the lava flood, slick ice. Damages (or slows) on a timer. */
UCLASS()
class WILDREACH_API AWRHazard : public AActor
{
	GENERATED_BODY()

public:
	AWRHazard();
	virtual void Tick(float DeltaSeconds) override;

	/** Server. A flood hurts everyone on the ground in the radius except those standing on a boss prop (anvils). */
	static AWRHazard* Spawn(UWorld* World, AActor* Source, const FVector& Location, float Radius, float Seconds, float DamagePerSecond, FName Kind);

	TWeakObjectPtr<AActor> Source;
	float Radius = 250.f;
	float DamagePerSecond = 8.f;
	FName Kind = TEXT("lava");   // lava, flood, ice

private:
	float Clock = 0.f;
	TWeakObjectPtr<AWRGroundMarker> Marker;
};

/** Arena prop a boss owns: Vulkarn's anvils (safe from floods, melt in phase 2), Skaldrith's ice pillars (block breath, shatter). */
UCLASS()
class WILDREACH_API AWRBossProp : public AActor
{
	GENERATED_BODY()

public:
	AWRBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_Kind) FName Kind = TEXT("anvil");
	float Radius = 150.f;

	void Setup(FName InKind);
	/** Server: smash it. */
	void Shatter();

private:
	UFUNCTION() void OnRep_Kind();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};

UENUM()
enum class EWRBossShape : uint8
{
	SelfCircle,    // around the boss
	TargetCircle,  // where the target stands when the windup starts (thrown / slammed)
	Cone,          // in front of the boss
	Line,          // straight ahead (grabs, lunges, charges)
};

/** One boss attack. Numbers come from tuning.json bosses.<boss>.<attack>.* with these as fallbacks. */
struct FWRBossAttack
{
	FName Id;
	EWRBossShape Shape = EWRBossShape::Cone;
	float Range = 400.f;      // reach (Line/Cone length, TargetCircle max throw distance)
	float Size = 300.f;       // radius (circles), half-angle in degrees (cones), half-width (lines)
	float Windup = 0.8f;
	float Damage = 40.f;
	float Poise = 30.f;
	float Cooldown = 6.f;
	float Buildup = 0.f;      // burn/frost buildup, by the boss's element
	int32 Repeats = 1;
	float RepeatGap = 0.55f;
	int32 MinPhase = 1;
	uint8 Height = 0;         // 0 = only on the ground, 1 = only airborne, 2 = either
	bool bMelee = true;       // blockable / parryable
	double ReadyAt = 0.0;
};

/**
 * Major boss or mini-boss. Phases by health, telegraphed attacks, a damage ledger (>= 10% of max
 * health earns personal loot), health that scales with the party (each tame counts), arena
 * reset when everyone leaves or dies. Each boss's counterplay lives in its Special* hooks.
 */
UCLASS()
class WILDREACH_API AWRBoss : public AWRCreature
{
	GENERATED_BODY()

public:
	AWRBoss(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float ModifyIncomingDamage(const FWRHit& Hit, float Damage) override;
	virtual void OnDamageTaken(const FWRHit& Hit, float Damage) override;
	virtual void NotifyAttackedBy(AActor* Attacker) override;
	virtual FString GetDisplayLabel() const override;

	bool IsMajor() const { return bMajor; }

	UPROPERTY(Replicated) int32 Phase = 1;
	UPROPERTY(Replicated) bool bEngaged = false;
	UPROPERTY(Replicated) bool bAirborne = false;
	UPROPERTY(Replicated) int32 Participants = 1;
	/** Obsidian Warden's shell, 0..1 (0 = broken, core exposed). */
	UPROPERTY(Replicated) float ShellFraction = 0.f;

	/** Server: who has dealt how much (player name -> damage; tames count for their owner). */
	TMap<FString, float> Ledger;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnDeath(AActor* Killer) override;

private:
	void BuildAttacks();
	FWRBossAttack MakeAttack(FName AttackId, EWRBossShape Shape, float Range, float Size, float Windup, float Damage, float Cooldown) const;
	void SpawnArena();
	void Engage();
	void ResetFight();
	void UpdateParticipants();
	AWRCharacterBase* PickBossTarget() const;
	void TickCombat(float DeltaSeconds);
	void StartAttack(FWRBossAttack& Attack, AWRCharacterBase* Target);
	void ResolveAttack();
	bool InShape(const FWRBossAttack& Attack, const FVector& Point) const;
	bool BreathBlocked(const FVector& Point) const;
	void SetPhase(int32 NewPhase);
	void SetAirborne(bool bAir);
	void Flood();
	void SpawnWisps();
	FGameplayTag BossElement() const;
	bool AnyArtifactModifiers() const;
	float Tune(const TCHAR* Key, float Fallback) const;

	bool bMajor = false;
	float SoloHealth = 3000.f;
	float ArenaRadius = 3000.f;
	TArray<FWRBossAttack> Attacks;
	TArray<TWeakObjectPtr<AWRBossProp>> Props;
	TArray<TWeakObjectPtr<AWRCreature>> Wisps;

	// Current attack.
	int32 Current = INDEX_NONE;
	int32 RepeatsLeft = 0;
	double ResolveAt = 0.0;
	double BusyUntil = 0.0;
	FVector AttackPoint = FVector::ZeroVector;
	FRotator AttackFacing = FRotator::ZeroRotator;

	double EmptySince = -1.0;
	double NextParticipantCheck = 0.0;
	double NextFloodAt = 0.0;
	double SpecialClock = 0.0;     // Ignessa landing / Pale Hunter blizzard / Skaldrith flight cycle
	double RevealedUntil = 0.0;
	double GroundedUntil = 0.0;
	double NextFootprint = 0.0;
	float WingHits = 0.f;
	float HealthBeforeHit = 0.f;
	float ShellHealth = 0.f;
	float MaxShell = 0.f;
	bool bWispsOut = false;
	bool bPhase3Ready = false;
	double PlungeUntil = 0.0;
};
