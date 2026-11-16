#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRInventory.h"
#include "WRSocial.generated.h"

class AWRCharacterBase;
class AWRPlayerController;
class UStaticMeshComponent;

/**
 * Small clans (server only, saved in the world's folder: Saved/Worlds/<world>/clans.json). Clanmates share claims, doors,
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
	/** Turn down a clan's invite (the invited player), or take one back (the leader). */
	FString Decline(const FString& Player, const FString& Clan);
	FString Uninvite(const FString& Player, const FString& Target);
	FString Describe(const FString& Player) const;
	/** The clan's leader ("" = no such clan). */
	FString LeaderOf(const FString& Clan) const;

	/** The current world's clans (the game mode loads them again for each world it starts). */
	void Load();
	void Save() const;
	/** Server: copy everyone's clan onto their PlayerState (replicated to clients). */
	void RefreshPlayerStates(const UWorld* World) const;

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
	/** Owner or a clanmate is connected. */
	static bool IsAnyoneOnline(const UWorld* World, const FString& Owner);

	/**
	 * Offline raid protection (cooks/cookpass-multiplayer.md 3.2; ARK's lesson): a base is protected only when none of its
	 * owners (the owner and their clan) is on, server.offlineArmMinutes have passed since the last of them left, and
	 * server.offlineQuietMinutes since it was last attacked (LastAttackedUtc, 0 = never). It ends the moment one logs in.
	 * Returns the state (0 off on this world, 1 one of them is home, 2 arming, 3 protected) and, arming, when it arms.
	 */
	enum : uint8 { ProtectOff = 0, ProtectHome = 1, ProtectArming = 2, ProtectOn = 3 };
	static uint8 ProtectionFor(const UWorld* World, const FString& Owner, int64 LastAttackedUtc, int64& OutArmsAtUtc);
	/** Server: a player left / joined (their leaving time arms their bases; it's kept in their account too). */
	static void NoteLeft(const FString& Player);
	static void NoteJoined(const FString& Player);
	/** When they last left (UTC seconds): this session's note, else their account's last seen, else when this world started. */
	static int64 LastLeftUtc(const FString& Player);
	/** Server, as a world starts: forget the last world's notes; leaving times count from now for anyone unknown. */
	static void ResetSession();
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

/** What a death leaves behind (Souls-like: only you can pick it up; drop/full loot: anyone), what a broken chest
 *  spills, and anything else dropped. Modelled (WRLootLook, cooks/cookpass-multiplayer.md 3.5): a satchel and bedroll,
 *  a stove-in crate, a tied sack. The owner of a souls satchel sees a column of light over it. */
UCLASS()
class WILDREACH_API AWRLootBag : public AActor
{
	GENERATED_BODY()

public:
	AWRLootBag();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** What it is (and so how it looks): a death's belongings, a broken chest's contents, anything else dropped. */
	enum : uint8 { KindDrop = 0, KindDeath = 1, KindSpill = 2 };

	/** Server: run the death penalty for a player who just died here. */
	static void ApplyDeathPenalty(AWRCharacterBase* Died, class AWRPlayerState* PS);
	/** Server: items in a bag where they fell that anyone can take, for building.spillMinutes (a chest's: KindSpill). */
	static AWRLootBag* Spill(UWorld* World, const FVector& At, const FString& Owner, const TArray<FWRItemStack>& Items, uint8 InKind = KindDrop);
	/** Server: take everything. */
	FString Loot(AWRPlayerController* Player);
	/** Server: its kind (its look follows on every machine) and how long it lasts (seconds; 0 keeps the life it has). */
	void Setup(uint8 InKind, float LifeSeconds);

	/** Server: the souls and loot satchels into the world save and back after a clean restart (with the time they had left). */
	static TSharedRef<class FJsonObject> SaveAll(UWorld* World);
	static void LoadAll(UWorld* World, const TSharedPtr<class FJsonObject>& Json);
	/** Server: write that list now (a death's bag made, taken from or gone; the world's own save). */
	static void SaveNow(UWorld* World, bool bClosing = false);

	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) bool bOwnerOnly = true;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 Kind = KindDrop;
	/** Server world time it goes (0: no end set); clients read it against the game state's server time. */
	UPROPERTY(Replicated) float ExpiresAt = 0.f;
	/** Its owner's main path, for the beacon's colour. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName OwnerPath;
	TArray<FWRItemStack> Items;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnRep_Look();
	/** Server: sit it on the ground under it, tilted to the slope. */
	void Settle();
	void BuildLook();
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Body;
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> Beacon;
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> BeaconLight;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> BeaconMat;
	int32 BuiltKind = -1;
};
