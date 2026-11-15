#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Components/ActorComponent.h"
#include "WRSporePath.generated.h"

class AActor;
class AWRCharacterBase;
class AWRCreature;
class AWRPlayerController;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
struct FWRHit;

/**
 * The Spore path's rules (cooks/biome-fungal.md 2.17-2.18; build-biome-fungal.md 2.17). Spores is the spender bar (100,
 * +20 a Spore capacity rank): it refills 5 a second, twice that standing in any spore cloud, and +3 for every planted
 * Spore you detonate. Planted Spores are a per-target stack owned by the planter (UWRSporeHost on the target: cyan
 * nodules every machine sees; 20 s, refreshed by a new plant; 10 a target, +1 a capacity rank, 5 on players), burst by
 * Detonate (8 + rot 10 each, a chain 0.05 s apart). The root passive Immune system (main path only): immune to poison and
 * rot. The passives the combat code asks about: Rot chance, Decay, Corrode, Lingering, Burst, Pandemic, Swarm, Host, Hive
 * mind, Thick spores, Regrowth spores, Toxic blood, Adrenal, Bloom body; the summons (Sporelings, the Fungal behemoth:
 * never tames, never saved, no body); Spore armor and Living colony; the combos with the built paths (Data/combos.json).
 * Numbers: tuning spores, sporePath, spells.<key>, wildbrands.infect_strike, nodeEffects spore.*, combos.json numbers.
 */
namespace WRSporePath
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Immune);         // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_SporeArmor);      // the spore coat: -15% damage taken
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_LivingColony);    // the capstone transform: a walking spore cloud
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SporeDrift);       // Wind + Spore 25: Spores reach farther (detonation and clouds)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_PlagueWind);       // Wind + Spore 50: your wind spells spread rot
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Combustion);       // Fire + Spore 25: burning Spores detonate in flame
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AshColony);        // Fire + Spore 50: sporelings immune to fire, their bites burn
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_StaticCloud);      // Lightning + Spore 25: your spore clouds shock
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_NeuralSpores);     // Lightning + Spore 50: lightning detonates Spores and stuns
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Mycorrhiza);       // Nature + Spore 25: your plants' blows plant Spores
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ForestFloor);      // Nature + Spore 50: plants and sporelings strengthen each other
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_HardyColony);      // Frost + Spore 25: sporelings shrug off cold
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_IceBloom);         // Frost + Spore 50: detonated Spores burst into ice shards
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_InfectedWounds);   // Blood + Spore 25: bleeding enemies spread Spores
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BloodColony);      // Blood + Spore 50: sporelings grow stronger on blood
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Bog);              // Tide + Spore 25: Spores grow faster on soaked enemies
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SwampColony);      // Tide + Spore 50: sporelings swim and soak
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FungalPuppets);    // Necromancy + Spore 25: Spore kills rise as fungal minions
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_EndlessColony);    // Necromancy + Spore 50: dead sporelings rise again once

	// ---------------------------------------------------------------- the path
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API int32 Rank(const AActor* Who, FName Node);
	WILDREACH_API int32 PointsIn(const AActor* Who);
	/** A number from the path's tuning (sporePath.<Key>). */
	WILDREACH_API float Num(const TCHAR* Key, float Fallback);

	// ---------------------------------------------------------------- Spores
	WILDREACH_API float SporesOf(const AActor* Who);
	WILDREACH_API float MaxSporesOf(const AActor* Who);
	WILDREACH_API bool HasSpores(const AActor* Who, float Amount);
	WILDREACH_API bool SpendSpores(AWRCharacterBase* Who, float Amount);
	WILDREACH_API void GainSpores(AWRCharacterBase* Who, float Amount);
	WILDREACH_API void NoteRefused(const AActor* Who);
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** Server, every tick for a survivor (the status component): capacity, the refill, Regrowth spores, the Living colony's
	 *  aura, Pandemic's jumps, the summons' upkeep (expiry, Hive mind). */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- planted Spores
	/** Server: plant N of By's Spores on Target (capped). Returns how many took. */
	WILDREACH_API int32 Plant(AWRCharacterBase* By, AWRCharacterBase* Target, int32 N);
	/** How many of By's Spores are on Target now (every machine reads the host's replicated counts). */
	WILDREACH_API int32 PlantedBy(const AActor* Target, const AActor* By);
	/** Server: burst every Spore By has planted within Range (bSoft: no damage, the Sporestalker's calm); Only: just on
	 *  that one. Returns how many burst. */
	WILDREACH_API int32 Detonate(AWRCharacterBase* By, float Range, bool bSoft = false, AWRCharacterBase* Only = nullptr);

	// ---------------------------------------------------------------- what the combat code asks
	/** Server: Rot chance's extra rot on Attacker's blows (and its +5 rot on weapon hits). */
	void ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit);
	/** Server: a blow landed (Infect strike's plants, the summons' bites, Toxic blood, the combos). */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Outgoing damage x: Corrode on a rotting target, Hive mind on its target, Blood colony on the bleeding. */
	WILDREACH_API float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Incoming damage x on Who: Spore armor. */
	WILDREACH_API float TakenScale(const AWRCharacterBase* Who, const FWRHit& Hit);
	/** Status buildup taken x (Immune system: none of poison or rot; Bloom body x0.9 a rank). */
	WILDREACH_API float BuildupTakenScale(const AActor* Who, int32 StatusId);
	/** Rot on a target from Source lasts x this (Lingering), hurts x this (Decay). */
	WILDREACH_API float RotSecondsScale(const AActor* Source);
	WILDREACH_API float RotDamageScale(const AActor* Source);

	/** Server: Infect strike is swinging: its blows in the next Seconds plant 3 Spores. */
	WILDREACH_API void MarkInfect(const AActor* Who, float Seconds);

	// ---------------------------------------------------------------- summons
	/** Server: a Sporeling (or the Fungal behemoth) of Owner's for Seconds; bFree: Host's, outside the cast's cost. */
	WILDREACH_API AWRCreature* Summon(AWRCharacterBase* Owner, FName Kind, float Seconds, const FVector& At);
	WILDREACH_API bool IsSummon(const AActor* Who);
	WILDREACH_API AWRCharacterBase* SummonOwner(const AActor* Who);
	WILDREACH_API int32 SummonCap(const AActor* Owner);
	/** Server: a creature died (Host's sporeling out of it, Endless colony, Fungal puppets). */
	void CreatureDied(AWRCreature* Dead, AActor* Killer);
	/** Server: a spore proc's look and sound on everyone. */
	WILDREACH_API void Proc(AActor* At, FName Fx, FName Sound, float Scale = 1.f);

	// ---------------------------------------------------------------- the Sporestalker's calm (path magic, fungal 2.12.1)
	/** Every machine: the prompt for Who at a Sporestalker knocked out or under 30% and rotting (10+ Spore points). */
	WILDREACH_API bool PathTamePrompt(const AWRCreature* Wild, const AActor* Who, FString& OutVerb);
	/** Server, E pressed (through AWRCreature::Feed): the hand goes on. True when it handled the press. */
	WILDREACH_API bool TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult);
	/** Server: E let go early ("world spathtame"). */
	WILDREACH_API void StopPathTame(AWRPlayerController* Player);
	/** The owning machine: this player has a hand on a Sporestalker (E's release must tell the server). */
	WILDREACH_API bool PathTameNear(const class APlayerController* Player);

	// ---------------------------------------------------------------- the body (cooks/biome-fungal.md 2.18)
	uint32 LookSig(const AActor* Hero);
	/** Every machine: the freckles glow only in the dark (caves, night), flare when you cast; at 100 the slow wave. */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS);
}

/** One planter's Spores on a target (replicated so every machine draws the nodules). */
USTRUCT()
struct FWRPlantedSpores
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Planter = nullptr;
	UPROPERTY() uint8 Count = 0;
	double Until = 0.0;
};

/** The Spores planted on a body: the counts by planter (replicated), the cyan nodules on its surface (every machine). */
UCLASS(ClassGroup=(Combat))
class WILDREACH_API UWRSporeHost : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRSporeHost();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	static UWRSporeHost* Get(const AActor* Actor);
	static UWRSporeHost* FindOrAdd(AActor* Actor);

	UPROPERTY(ReplicatedUsing=OnRep_Planted) TArray<FWRPlantedSpores> Planted;
	int32 Total() const { int32 N = 0; for (const FWRPlantedSpores& P : Planted) N += P.Count; return N; }
	int32 Of(const AActor* By) const { for (const FWRPlantedSpores& P : Planted) if (P.Planter == By) return P.Count; return 0; }
	/** Server: set By's count (0 removes), refreshing its 20 s. */
	void SetCount(AActor* By, int32 N, double Until);
	void Changed() { OnRep_Planted(); if (AActor* O = GetOwner()) O->ForceNetUpdate(); }

private:
	UFUNCTION() void OnRep_Planted();
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Nodules;
	int32 Shown = -1;
};
