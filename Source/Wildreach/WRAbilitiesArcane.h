#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRAbilities.h"
#include "WRAbilitiesArcane.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USphereComponent;

/**
 * The Arcane path's abilities (cooks/biome-crystal.md 2.17; build-biome-crystal.md P2.9). Mana is the spender: each pays
 * spells.<key>.manaCost as it goes off; a press you can't pay flashes the Mana bar. Spells wind up by their shape (Quick
 * cast and Quick mind shorten it) and may Echo (20%: the same spell again 0.6 s later, free, at the same aim; never the
 * capstones or the shields). Numbers: tuning spells.<key>, spells.shapes.<shape>, wildbrands.<node>, arcane.*; the
 * path's rules are WRArcanePath.h. Registered in UWRAbilityRegistry::ClassForNode under "arcane".
 */

/** An Arcane spell: Mana, the shape's wind-up (rooted or walking at half speed), an optional charge, then CastArcane. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_ArcaneSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_ArcaneSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Server: the spell goes off (after the wind-up). Power: x1..1.5 from a held charge. bEcho: Echo's free repeat. */
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) {}
	/** Where the cast aims (fixed at the wind-up's end, so an Echo repeats it exactly). */
	virtual void TakeAim(AWRCharacterBase* Caster) {}
	FString SpellKey;
	FString ShapeName = TEXT("bolt");
	/** Echo may repeat it (not the capstones, not the shields). */
	bool bEchoes = true;
	float Spell(const TCHAR* Field, float Fallback) const;
	float ManaCost() const;
	/** Where a ground spell lands: under the lock (within Range), else where the crosshair meets the ground. */
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
	/** A hit from this spell (Arcane element, the path, a spell). */
	FWRHit ArcaneHit(AWRCharacterBase* Caster, float Damage, float Mark) const;
	/** Glyph power on runes (+10% a rank). */
	float Glyph(const AWRCharacterBase* Caster) const;
	FVector AimPoint = FVector::ZeroVector;
	FVector AimDir = FVector::ForwardVector;
	TWeakObjectPtr<AWRCharacterBase> AimLock;

private:
	void CastNow();
	UFUNCTION() void OnChargeReleased(float TimeHeld);
	float Charge = 1.f;
	bool bCharging = false;
	double ChargeStarted = 0.0;
};

/** Arcane missile (trunk starter): a violet bolt, 14 damage, mark 25, 2,800 cm/s, 8 Mana, cd 0.8. */
UCLASS() class WILDREACH_API UWRGA_ArcaneMissile : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_ArcaneMissile();
protected:
	virtual void TakeAim(AWRCharacterBase* Caster) override;
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Barrier (Ward): a 30 HP shield for 8 s on yourself (Hardened ward +20% a rank); with Ward allies, on the ally nearest
 *  the crosshair within 15 m in sight (tames count); 20 Mana, cd 12. */
UCLASS() class WILDREACH_API UWRGA_Barrier : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_Barrier();
protected:
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Arcane blast (Sorcery): a nova where you aim within 12 m, r 300, 32 damage, stance x2; chargeable to x1.5 at 1 s; 25
 *  Mana, cd 8. */
UCLASS() class WILDREACH_API UWRGA_ArcaneBlast : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_ArcaneBlast();
protected:
	virtual void TakeAim(AWRCharacterBase* Caster) override;
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Rune trap (Rune): a visible hexafoil rune r 300 where you aim, 40 when an enemy steps on it, 30 s (Lasting runes), at
 *  most 3; never inside another clan's claim; 15 Mana, cd 10. */
UCLASS() class WILDREACH_API UWRGA_RuneTrap : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_RuneTrap();
protected:
	virtual void TakeAim(AWRCharacterBase* Caster) override;
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Inscribe (Rune): rooted 0.6 s, a rune drawn down your blade: +8 arcane on every weapon blow for 30 s; 30 Mana, cd 30. */
UCLASS() class WILDREACH_API UWRGA_Inscribe : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_Inscribe();
protected:
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Aegis (Ward capstone): rooted 1 s drawing the eight-armed stave; a dome r 500 for 8 s that stops every projectile
 *  crossing it (Reflect: reflectable ones bounce back); a catapult stone breaks it; beams and melee pass; 50 Mana, cd 60. */
UCLASS() class WILDREACH_API UWRGA_Aegis : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_Aegis();
protected:
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Starfall (Sorcery capstone): 12 stars over r 700 where you aim, 18 each, each landing point glinting 1.2 s first;
 *  60 Mana, cd 50. Meteor storm adds burn, Blood star bleed. */
UCLASS() class WILDREACH_API UWRGA_Starfall : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_Starfall();
protected:
	virtual void TakeAim(AWRCharacterBase* Caster) override;
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Rune circle (Rune capstone): a great circle r 600 for 15 s where you aim: allies inside +15% damage, enemies inside
 *  mark 10 a second (Faerie ring: plants that heal allies 4 a second); 60 Mana, cd 60. */
UCLASS() class WILDREACH_API UWRGA_RuneCircle : public UWRGA_ArcaneSpell
{
	GENERATED_BODY()
public:
	UWRGA_RuneCircle();
protected:
	virtual void TakeAim(AWRCharacterBase* Caster) override;
	virtual void CastArcane(AWRCharacterBase* Caster, float Power, bool bEcho) override;
};

/** Runic strike (Wildbrand, trunk): any melee weapon, x1.3, mark 40, stance x1.5; 15 Mana, cd 2.5. */
UCLASS() class WILDREACH_API UWRGA_RunicStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_RunicStrike();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * An Arcane ground working (server-owned, replicated): Kind rune (a Rune trap: a faint hexafoil on the ground, visible to
 * everyone, that bursts when an enemy steps on it; within 8 m of its caster's other runes a violet thread joins them with
 * Rune link), dome (Aegis: a faceted violet dome with the eight-armed stave on its face, stopping projectiles that cross
 * it), circle (Rune circle: a great ring of hexafoils and compass lines). Drawn on every machine from Kind, Radius, Born
 * and Until; it rises over 0.3 s and fades over 0.5 s, never popping.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRArcaneField : public AActor
{
	GENERATED_BODY()

public:
	AWRArcaneField();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) float Radius = 300.f;
	UPROPERTY(Replicated) float Born = 0.f;
	UPROPERTY(Replicated) float Until = 0.f;
	/** Whose working (runes link to their own; allies of theirs are buffed, enemies hurt). */
	UPROPERTY(Replicated) TObjectPtr<AWRCharacterBase> Caster;
	/** A rune drawn by a caster with Rune link: it threads to their other linked runes within 8 m. */
	UPROPERTY(Replicated) bool bLinked = false;

	// Server.
	float Damage = 40.f;
	float Frost = 0.f;
	bool bReflect = false;
	bool bFaerie = false;
	/** All live fields on this machine (for runes' threads, domes' stops). */
	static TArray<TWeakObjectPtr<AWRArcaneField>>& All();
	/** Server: a catapult stone or the end: it goes, fading. */
	void Finish(bool bShattered);
	float Fade(float Now) const;
	float ServerNow() const;

private:
	void BuildLook();
	void StepRune(float Dt);
	void StepDome(float Dt);
	void StepCircle(float Dt);
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Look;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	TMap<TWeakObjectPtr<AActor>, float> LastProjectileDist;
	TMap<TWeakObjectPtr<AActor>, double> ThreadHitAt;
	TSet<TWeakObjectPtr<AWRCharacterBase>> InCircle;
	float StepClock = 0.f;
	bool bDone = false;
};

/**
 * The Arcane path's world-wide upkeep: on the server the shields' clocks (every survivor and tame); on every drawing
 * machine the shield film (a faceted violet shell round anyone shielded, from the replicated tag) and Rune link's threads
 * between a caster's runes within 8 m.
 */
UCLASS()
class WILDREACH_API UWRArcaneWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRArcaneWorld, STATGROUP_Tickables); }
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	struct FFilm { TWeakObjectPtr<UProceduralMeshComponent> Mesh; TWeakObjectPtr<UMaterialInstanceDynamic> Glow; float Shown = 0.f; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FFilm> Films;
	struct FThread { TWeakObjectPtr<UProceduralMeshComponent> Mesh; TWeakObjectPtr<AWRArcaneField> A, B; };
	TArray<FThread> Threads;
	float ScanClock = 0.f;
	void StepFilms(float Dt);
	void StepThreads();
};
