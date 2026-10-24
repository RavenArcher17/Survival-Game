#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRInventory.h"
#include "WRSocial.generated.h"

class AWRCharacterBase;
class AWRPlayerController;
class UStaticMeshComponent;

/**
 * Small clans (server only, saved to Saved/ServerDB/clans.json). Clanmates share claims, doors,
 * chests and tames, and can't hurt each other. Size limit comes from server.clanSizeLimit (0 = none).
 */
class WILDREACH_API FWRClans
{
public:
	static FWRClans& Get();

	FString ClanOf(const FString& Player) const;
	bool SameClan(const FString& A, const FString& B) const;
	TArray<FString> Members(const FString& Clan) const;

	/** Each returns a message for the player. */
	FString Create(const FString& Player, const FString& Clan);
	FString Invite(const FString& Player, const FString& Target);
	FString Join(const FString& Player, const FString& Clan);
	FString Leave(const FString& Player);
	FString Kick(const FString& Player, const FString& Target);
	FString Describe(const FString& Player) const;

	void Load();
	void Save() const;

private:
	FWRClans() { Load(); }
	struct FClan { FString Leader; TArray<FString> Members; TArray<FString> Invites; };
	TMap<FString, FClan> Clans;
};

/** Sharing, hostility and server rules in one place. */
struct WILDREACH_API FWRSocialRules
{
	/** Owner (or no owner) or a clanmate. */
	static bool CanShare(const FString& Owner, const FString& Player);
	/** Survivor vs survivor: PvP setting, clans, safe zones, PvP inside boss arenas. */
	static bool PlayersCanFight(const AWRCharacterBase* A, const AWRCharacterBase* B);
	/** Owner or a clanmate is connected (offline base protection). */
	static bool IsAnyoneOnline(const UWorld* World, const FString& Owner);
	static bool IsInSafeZone(const UWorld* World, const FVector& Location);
	/** Server preset string setting (deathPenalty, raids). */
	static FString Setting(const TCHAR* Key, const TCHAR* Fallback);
};

/** Spawn safe zone: no PvP, no building, wild creatures won't chase you in. */
UCLASS()
class WILDREACH_API AWRSafeZone : public AActor
{
	GENERATED_BODY()

public:
	AWRSafeZone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Replicated, Category="Safe zone") float Radius = 3000.f;
};

/** What a death leaves behind (Souls-like: only you can pick it up; drop/full loot: anyone). */
UCLASS()
class WILDREACH_API AWRLootBag : public AActor
{
	GENERATED_BODY()

public:
	AWRLootBag();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: run the death penalty for a player who just died here. */
	static void ApplyDeathPenalty(AWRCharacterBase* Died, class AWRPlayerState* PS);
	/** Server: take everything. */
	FString Loot(AWRPlayerController* Player);

	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) bool bOwnerOnly = true;
	TArray<FWRItemStack> Items;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};
