#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "WRPathTypes.h"
#include "WRPathComponent.generated.h"

class AWRCharacter;

/**
 * A survivor's paths: main path (picked at creation), subclass, saved progress for every path
 * ever used, and the 8 quick slots. Lives on the PlayerState so it survives death. All changes
 * go through server-validated calls; the owner gets a replicated copy for the HUD.
 */
UCLASS(ClassGroup=(Paths))
class WILDREACH_API UWRPathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRPathComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Finds the path component for a character (via its PlayerState); null for creatures. */
	static UWRPathComponent* FindFor(const AActor* Character);

	// --- Server API (each returns an empty string on success, else why it failed) ---
	FString SetMainPath(FName PathId);
	/** Developer override: change the main path even after creation. */
	void DevSetMainPath(FName PathId);
	FString SetSubPath(FName PathId);
	FString BuyNode(FName PathId, FName NodeId);
	/** Buys nodes in tree order until Points are spent or nothing more can be bought (testing aid). */
	int32 AutoSpend(FName PathId, int32 Points);
	void AwardXP(FName PathId, int32 Amount);
	/** Gives exactly enough XP for this many more points (cheat). */
	void GivePoints(FName PathId, int32 Points);

	// --- The endgame (WRDeeds; cooks/build-endgame.md 2.1, 2.2) ---
	/** Prestige: the path's ranks and XP go back to the start (its starter stays). Returns the ranks it had (its layout). */
	TArray<FWRNodeRank> ReturnPath(FName PathId);
	/** Respec (the Heron's draught): the path's ranks go (the starter stays); its XP stays, so every point is free. */
	bool UnlearnPath(FName PathId);
	/** Buys a saved layout back, in its order, as far as points allow. Returns how many ranks it bought. */
	int32 RestoreLayout(FName PathId, const TArray<FWRNodeRank>& Layout);
	/** The held weapon's soul art came or went: the quick slots and grants follow. */
	void RefreshSoulArt() { Changed(); }

	/** Server: push grants (abilities, stats, passives, combos, level) onto the current pawn. */
	void ApplyToCharacter(AWRCharacter* Character);

	// --- Queries (valid on server and owning client) ---
	FName GetMainPath() const { return MainPath; }
	FName GetSubPath() const { return SubPath; }
	bool IsActivePath(FName PathId) const { return !PathId.IsNone() && (PathId == MainPath || PathId == SubPath); }
	const FWRPathProgress* GetProgress(FName PathId) const;
	int32 GetSpent(FName PathId) const;
	int32 GetEarned(FName PathId) const;
	int32 GetTotalSpentActive() const;
	int32 GetCharacterLevel() const;
	const TArray<FName>& GetQuickSlots() const { return QuickSlots; }

	/**
	 * Sum of a stat over the character's owned stat nodes in its active paths, from
	 * tuning "nodeEffects" (e.g. Stat(Attacker, "burnDamage") -> 0.45 with 3 ranks of Searing heat).
	 * 0 for creatures. Works on the server and the owning client (progress replicates to the owner).
	 */
	static float Stat(const AActor* Character, FName StatName);
	/** Owns this node (rank > 0) in an active path: passive ability nodes check this. */
	static bool HasNode(const AActor* Character, FName PathId, FName NodeId);

	/** Rate multiplier for path XP (server setting; -xpmult=N on the command line). */
	static float XPMultiplier();

	// --- Persistence (server) ---
	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);
	bool IsDirty() const { return bDirty; }
	void ClearDirty() { bDirty = false; }

	static constexpr int32 NumQuickSlots = 8;

private:
	FWRPathProgress& FindOrAddProgress(FName PathId);
	void RefreshQuickSlots();
	void Changed();
	/** The starter spell of the main path and the subclass, if not owned yet; true if any was given. */
	bool GrantStarters();

	UPROPERTY(Replicated) FName MainPath;
	UPROPERTY(Replicated) FName SubPath;
	UPROPERTY(Replicated) TArray<FWRPathProgress> Progress;
	/** "path.node" per slot, None when empty. */
	UPROPERTY(Replicated) TArray<FName> QuickSlots;
	/** The ability a Soul art took the last quick slot from (put back there when the awakened weapon leaves the hand). */
	FName SoulDisplaced;

	bool bDirty = false;

	// Server-only bookkeeping of what we granted, so a re-apply removes stale grants.
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;
	FGameplayTagContainer GrantedTags;
};
