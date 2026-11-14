#pragma once
// Artifacts (FULL-GAME 8.3; cooks/endgame.md 2.6; cooks/build-portals-artifacts.md 2.12-2.14). Charms on three belt
// hooks; each has a Weight (I bone .. V starsteel) and one weird rule, written as a geis, that binds only its wearer.
// Artifact Level (AL) = the hooked Weights' sum (capped by the server). The party's highest AL sets the shared penalty
// (damage taken from creatures, bosses and the fold's traps) and rewards (loot, part chance, gear quality, path XP,
// artifact drops, Scarred parts at AL 5+, high Weights at AL 10+) and opens bosses' hidden third phases. PvE only:
// player damage pauses them 30 s; safe zones pause them. The band is the party beyond the clan. Data: Data/artifacts.json.
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRArtifacts.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCharacterBase;
class AWRBoss;
class FJsonObject;
struct FWRHit;

USTRUCT()
struct FWRArtifactRep
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() uint8 Weight = 1;
	bool IsEmpty() const { return Id.IsNone(); }
};

USTRUCT()
struct FWRGeisRecord
{
	GENERATED_BODY()
	UPROPERTY() FName Boss;
	/** The highest AL you've killed it at (with your 10%). */
	UPROPERTY() uint8 Best = 0;
};

/** A player's artifacts (on their PlayerState, saved in their file). */
UCLASS(ClassGroup=(Endgame))
class WILDREACH_API UWRArtifactComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWRArtifactComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static UWRArtifactComponent* For(const AActor* PlayerOrStateOrPawn);

	/** The three hooks (an empty Id is a free hook). Everyone sees the charms on your belt. */
	UPROPERTY(Replicated) TArray<FWRArtifactRep> Hooks;
	/** Server time the charms come back after player damage (they show grey till then). */
	UPROPERTY(Replicated) float PausedUntil = 0.f;
	/** The pouch: artifacts you carry but don't wear. Yours only. */
	UPROPERTY(Replicated) TArray<FWRArtifactRep> Pouch;
	UPROPERTY(Replicated) TArray<FWRGeisRecord> Records;
	/** Titles earned (a boss's Bane at AL 15). */
	UPROPERTY(Replicated) TArray<FString> Titles;
	/** Your band (the others in it) and who has asked you to join theirs. */
	UPROPERTY(Replicated) TArray<FString> Band;
	UPROPERTY(Replicated) TArray<FString> BandInvites;
	/** Tying a charm on (2 s): server time it's done (0: not), which hook, from which pouch slot (-1: taking it off). */
	UPROPERTY(Replicated) float HookingUntil = 0.f;
	UPROPERTY(Replicated) int8 HookingSlot = -1;
	int32 HookingFrom = -1;
	FVector HookingAt = FVector::ZeroVector;

	/** The hooked Weights' sum, capped by the server (0 with the cap off). */
	int32 RawAL() const;
	bool IsPaused() const;
	bool Wears(FName Rule) const;

	// --- Server ---
	FString Hook(int32 PouchIndex, int32 Slot);
	FString Unhook(int32 Slot);
	FString Fuse(int32 A, int32 B);
	FString Salvage(int32 Index);
	void Give(FName Id, uint8 Weight, const FString& Why);
	/** Server: state the rules keep. */
	double LastKillAt = -100.0;
	float HollowLost = 0.f;
	double NextBoltAt = 0.0;
	double BoltAt = 0.0;
	FVector BoltSpot = FVector::ZeroVector;
	float FallTopZ = 0.f;
	bool bFalling = false;
	bool bHagSpent = false;
	float FightCheckClock = 0.f;
	double LastFightAt = 0.0;
	TSet<TWeakObjectPtr<AActor>> GraveBosses;
	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& O);
private:
	void StepRules(float Dt);
	void OnRested(AWRPlayerState* PS);
	FDelegateHandle RestHandle;
};

namespace WRArtifacts
{
	WILDREACH_API TSharedPtr<FJsonObject> Data();
	WILDREACH_API TSharedPtr<FJsonObject> Def(FName Id);
	WILDREACH_API float Num(const FString& Path, float Fallback);
	WILDREACH_API FString Name(FName Id);
	WILDREACH_API FString MetalName(int32 Weight);
	WILDREACH_API FColor MetalColour(int32 Weight);
	/** Your own effective AL now (0 while paused or with the cap off). */
	WILDREACH_API int32 AL(const AActor* Who);
	/** The party's highest AL (in a fold: everyone in it; at a boss: your party in its reach; else your party near). */
	WILDREACH_API int32 PartyAL(const AActor* Who);
	/** The wearer has this rule on (hooked and not paused). */
	WILDREACH_API bool Has(const AActor* Who, FName Rule);
	WILDREACH_API float RuleNum(const AActor* Who, FName Rule, const TCHAR* Field, float Fallback);

	// --- Shared numbers ---
	/** Damage a player takes from creatures, bosses and the fold's traps (never from players). */
	WILDREACH_API float DamageTakenScale(const AWRCharacterBase* Target, const AWRCharacterBase* Attacker, const FWRHit& Hit);
	WILDREACH_API float LootScale(const AActor* Who);
	WILDREACH_API float XPScale(const AActor* Who);
	WILDREACH_API float PartChanceBonus(const AActor* Who);
	/** A kill's drop of a boss part comes Scarred (AL 5+, 25%). */
	WILDREACH_API bool RollScarred(const AActor* Who);
	/** Dropped gear steps up a quality (2% an AL). */
	WILDREACH_API bool RollQualityStep(const AActor* Who);
	/** Hidden phase 3 opens: the party with the most of the ledger has AL 1+ (and the server allows artifacts). */
	WILDREACH_API bool PhaseThreeOpen(const AWRBoss* Boss);

	// --- The party ---
	WILDREACH_API TArray<FString> BandOf(const AWRPlayerState* PS);
	WILDREACH_API TArray<FString> BandOfName(UWorld* World, const FString& Player);
	/** Same clan or same band. */
	WILDREACH_API bool InParty(UWorld* World, const FString& A, const FString& B);

	// --- Hooks (server) ---
	WILDREACH_API void NotePvP(AActor* A, AActor* B);
	/** A blow the wearer's rule stops (Grave Count: you died in this boss fight). */
	WILDREACH_API bool BlocksHit(const AWRCharacterBase* Target, const FWRHit& Hit);
	/** After a blow landed on a player (Rime Ring, Hollow Coin, the Hag's Offer, Forge-brand on the attacker). */
	WILDREACH_API void OnHitLanded(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	WILDREACH_API void OnKilled(AWRCharacterBase* Victim, AActor* Killer);
	/** A major boss died: drops, onboarding, geis records. */
	WILDREACH_API void OnBossKilled(AWRBoss* Boss);
	/** A portal's artifact roll for this player (empty: none). */
	WILDREACH_API FString PortalDrop(AWRPlayerState* PS, FName Biome, int32 Rank, float BaseChance);
	/** The scale on a push a player takes (Gale Feather) and whether a heavy blow knocks them down. */
	WILDREACH_API float KnockScale(const AActor* Victim);
	/** A heal reaching this player (No healing, Leech Tooth on potions, Open Vein's cap). Returns the heal to give. */
	WILDREACH_API float HealFilter(const AWRCharacterBase* Who, float Heal, bool bPotion);
	/** A path ability was used (Blood-price, Wardcarver's Chime). */
	WILDREACH_API void OnAbilityUsed(AWRCharacterBase* Who, bool bSpell);
	/** Light radius scale (Mourner's Veil). */
	WILDREACH_API float LightScale(const AActor* Who);
	/** Stamina regen scale (Bellows Lung, Undertow Pearl). */
	WILDREACH_API float StaminaRegenScale(const AWRCharacterBase* Who);
	/** Creature aggro range on this player (Connla's challenge). */
	WILDREACH_API float AggroScale(const AActor* Who);

	/** Server: "art ..." and "band ..." commands from the screens. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Server, every second: bands drop members gone bandIdleMinutes and every list follows. */
	WILDREACH_API void TickBands(UWorld* World);
	/** Within reach of an alchemy table (fusing, salvage). Every machine. */
	WILDREACH_API bool AtAlchemyTable(const AActor* Who);
}
