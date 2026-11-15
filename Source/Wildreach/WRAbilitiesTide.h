#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesTide.generated.h"

class UProceduralMeshComponent;

/**
 * The Tide path's abilities (cooks/biome-ocean.md 2.17; build-biome-ocean.md 2.16). Flow is the spender: each pays
 * spells.<key>.flowCost as it goes off; a press you can't pay flashes the Flow bar. Numbers: tuning spells.<key>,
 * spells.shapes.<shape>, wildbrands.<node>, tide.path.*; the path's rules are WRTidePath.h. Registered in
 * UWRAbilityRegistry::ClassForNode under "tide". Water bolts fly at full speed under water (everyone else's slow).
 */

/** A Tide spell: Flow, the shape's wind-up (rooted or walking), then CastTide on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_TideSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_TideSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void CastTide(AWRCharacterBase* Caster) {}
	virtual void TakeAim(AWRCharacterBase* Caster);
	FString SpellKey;
	FString ShapeName = TEXT("bolt");
	/** A rooted cast holds the Flow drain while it plays (Maelstrom, High tide). */
	bool bHoldsDrain = false;
	float Spell(const TCHAR* Field, float Fallback) const;
	float FlowCost() const;
	FWRHit TideHit(AWRCharacterBase* Caster, float Damage, float Soak) const;
	FVector AimDir = FVector::ForwardVector;
	TWeakObjectPtr<AWRCharacterBase> AimLock;
};

/** Water bolt (trunk starter): 15, soak 35, 2,600 cm/s (full speed under water), 6 Flow, cd 1.0; homes on creatures. */
UCLASS() class WILDREACH_API UWRGA_WaterBolt : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_WaterBolt();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Riptide pull (Undertow): a 30 degree cone 12 m out pulls enemies 5 m toward you (+1 m a Drag rank; at Drag 3 the
 *  highland come half way); not bosses; soak 20; lowland stagger 0.5 s as they arrive. 15 Flow, cd 10. */
UCLASS() class WILDREACH_API UWRGA_RiptidePull : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_RiptidePull();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Whirlpool (Undertow): a 3.5 m pool for 6 s at the aimed ground that holds lowland and midland, 6 a second, soak 15
 *  a second; highland slowed x0.7 inside; a column of water on land, a vortex in water. 30 Flow, cd 18. */
UCLASS() class WILDREACH_API UWRGA_Whirlpool : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_Whirlpool();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Maelstrom (Undertow capstone): a 10 m whirlpool for 8 s pulling everything in at 3 m/s, 12 a second, soak 20 a second;
 *  lowland and midland at its centre take x2; highland pulled at half speed; bosses soaked, never pulled. 60 Flow, cd 60. */
UCLASS() class WILDREACH_API UWRGA_Maelstrom : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_Maelstrom();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Healing wave (Tidecaller): a wave 6 m wide that rolls 12 m from you, healing every ally it passes 35 (Mending tide),
 *  passing through enemies harmlessly. 25 Flow, cd 12. */
UCLASS() class WILDREACH_API UWRGA_HealingWave : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_HealingWave();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** High tide (Tidecaller capstone): a great wave 10 m wide rolling 18 m at 10 m/s: allies healed 60, lowland and
 *  midland knocked back 6 m, highland staggered 1 s, bosses 40 stance. 60 Flow, cd 90. */
UCLASS() class WILDREACH_API UWRGA_HighTide : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_HighTide();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Cleanse (Tidecaller): the ally nearest your crosshair within 15 m (or you) loses every status and buildup but
 *  corruption. 20 Flow, cd 15. */
UCLASS() class WILDREACH_API UWRGA_Cleanse : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_Cleanse();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Tidal shield (Tidecaller): a 35 HP water shield on the ally nearest your crosshair (or you) for 8 s; when it breaks
 *  it bursts, soaking attackers within 2 m. 20 Flow, cd 15. */
UCLASS() class WILDREACH_API UWRGA_TidalShield : public UWRGA_TideSpell
{
	GENERATED_BODY()
public:
	UWRGA_TidalShield();
protected:
	virtual void CastTide(AWRCharacterBase* Caster) override;
};

/** Wave cut (trunk Wildbrand): the weapon swept in a curl of water, x1.25, soak 45, stance x1.5; under water and riding. */
UCLASS() class WILDREACH_API UWRGA_WaveCut : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_WaveCut();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * Harpoon throw (Harpooner Wildbrand, a spear in hand): the spear thrown on a line (27 m/s, 18 m of line), x1.5, soak 30,
 * stance x2, bleed by Barbed tip. Pressed again within 1.5 s of a hit: the reel. Lowland and midland come to you; a bigger
 * body stays put, and with Tow you're pulled to it instead (or to a wall or rock the harpoon struck, within 18 m); held,
 * with Anchor, a highland body is pinned 3 s (a mini slowed x0.5; a mooring stone or a cleat within 3 m makes it a full
 * tether). Leviathan's hook (capstone): the hook becomes a chain and drags a boss 3 m and staggers it 1.2 s (a mini 5 m and
 * 2 s), once each 30 s a boss. 25 Flow, cd 8 (Quick reel -1 s a rank). Works under water.
 */
UCLASS() class WILDREACH_API UWRGA_HarpoonThrow : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_HarpoonThrow();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	/** The line this survivor's last throw left waiting for its reel (both machines see it: it replicates). */
	class AWRHarpoonLine* PendingLine(const FGameplayAbilityActorInfo* ActorInfo) const;
	void Throw(AWRCharacterBase* Caster);
	void Reel(AWRCharacterBase* Caster, AWRHarpoonLine* Line);
	UFUNCTION() void OnReelReleased(float TimeHeld);
	TWeakObjectPtr<AWRHarpoonLine> Reeling;
	bool bWaitingRelease = false;
};

/**
 * A harpoon's line (shared: the Tide's Harpoon throw and the Whaler's harpoon): a rope (a chain with Leviathan's hook) from
 * the thrower's hand to where the barb went in (a body, or a wall), drawn on every machine as a sagging cable that pulls
 * tight when reeled, rigid and glowing sea-green when anchored. It waits ReelWindow s for its reel, then hangs for
 * Lifetime s (the Reef Tyrant's tether rules read it), and breaks when it has taken BreakPull of pull or its holder gets
 * too far (LineRange x 1.5). The server owns its state; TieTo makes it a tether on a mooring stone or a cleat.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRHarpoonLine : public AActor
{
	GENERATED_BODY()

public:
	AWRHarpoonLine();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) TObjectPtr<AActor> Holder;
	UPROPERTY(Replicated) TObjectPtr<AActor> Struck;
	UPROPERTY(Replicated) FVector_NetQuantize StuckAt = FVector::ZeroVector;    // a wall's point, or the body's offset
	UPROPERTY(Replicated) bool bChain = false;
	UPROPERTY(Replicated) bool bTaut = false;
	UPROPERTY(Replicated) bool bAnchored = false;
	/** Tied off to a mooring stone or a cleat (the line's land end): the struck body is tethered to TiedAt. */
	UPROPERTY(Replicated) bool bTied = false;
	UPROPERTY(Replicated) FVector_NetQuantize TiedAt = FVector::ZeroVector;
	UPROPERTY(Replicated) float BornAt = 0.f;
	/** Server: how much pull it has taken (it breaks at BreakPull), when it waits no more for the reel. */
	float Pull = 0.f;
	float ReelUntil = 0.f;
	float Lifetime = 20.f;
	float BreakPull = 600.f;
	bool bReeled = false;

	static AWRHarpoonLine* Make(AWRCharacterBase* Holder, AActor* Struck, const FVector& At, bool bChain);
	/** The end in the world (the body's point, or the wall's). */
	FVector FarEnd() const;
	FVector NearEnd() const;
	/** Server: tie the line's held end to a point (a mooring stone's ring): the holder lets go, the body is tethered. */
	void TieTo(const FVector& Ring);
	/** Server: some pull taken (the body straining against an anchor or a tether). */
	void AddPull(float Amount);
	/** Server: done with it (reeled in, broke, let go). */
	void Cut(bool bSnap);
	static AWRHarpoonLine* HeldBy(const AActor* Holder);
	static TArray<AWRHarpoonLine*> On(const AActor* Struck);

private:
	void DrawLine();
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Cable;
	float DrawClock = 0.f;
};

/**
 * A travelling wave (Healing wave, High tide): a wall Width cm wide rolling Length cm along Dir at Speed from Origin,
 * drawn on every machine from the replicated start (a curl of water and its spray); the server touches each body once.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRTideWave : public AActor
{
	GENERATED_BODY()

public:
	AWRTideWave();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FVector_NetQuantize Origin = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal Dir = FVector::ForwardVector;
	UPROPERTY(Replicated) float Width = 600.f;
	UPROPERTY(Replicated) float Length = 1200.f;
	UPROPERTY(Replicated) float Speed = 900.f;
	UPROPERTY(Replicated) float StartAt = 0.f;
	UPROPERTY(Replicated) bool bHigh = false;
	UPROPERTY(Replicated) bool bThunder = false;
	TWeakObjectPtr<AWRCharacterBase> Caster;
	float Heal = 35.f;

	static AWRTideWave* Launch(AWRCharacterBase* Caster, bool bHigh, float Width, float Length, float Speed, float Heal);

private:
	void Touch(AWRCharacterBase* Who);
	TSet<TWeakObjectPtr<AActor>> Touched;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Look;
	bool bLookBuilt = false;
};

/** A whirlpool (Whirlpool, Maelstrom; the Swamp guardian's pull is the path's): its swirl drawn on every machine, its
 *  holds, pulls, damage and soak on the server. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRTideField : public AActor
{
	GENERATED_BODY()

public:
	AWRTideField();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) bool bMaelstrom = false;
	UPROPERTY(Replicated) float Radius = 350.f;
	UPROPERTY(Replicated) float EndAt = 0.f;
	UPROPERTY(Replicated) bool bBoiling = false;
	TWeakObjectPtr<AWRCharacterBase> Caster;

private:
	float Clock = 0.f;
	TSet<TWeakObjectPtr<AActor>> Frozen;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Look;
	bool bLookBuilt = false;
};
