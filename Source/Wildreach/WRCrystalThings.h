#pragma once

// The Crystal caves' live things (cooks/biome-crystal.md 2.1, 2.6, 2.11, 2.20; cooks/build-biome-crystal.md 2.6-2.7):
// the replicated state (each hall's keystone light), the spots you press E at (the halls' keystone chimes, the
// keepsakes), and the noise model's network half. Sound makes light: every noise the server hears is sent to every
// machine (AWRCrystalState::MulticastNoise), which lights the crystals round it (UWRCrystalWorld::Light); footsteps are
// worked out on each machine from the characters it sees (no traffic). The server's listeners (Echobats, chandeliers,
// the Vault door, the Echo Warden) take the same noises through UWRCrystalWorld::OnNoise.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCrystalThings.generated.h"

class AWRPlayerController;
class AWRCharacterBase;
class UPointLightComponent;

/** The caves' shared state, one per world (server-spawned, replicated to all). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCrystalState : public AActor
{
	GENERATED_BODY()

public:
	AWRCrystalState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AWRCrystalState* Get(const UObject* WorldContext);

	/** Per hall (FWRCrystalData::Halls index): the game hour (GetTotalHours) its struck keystone chime holds it lit to. */
	UPROPERTY(Replicated) TArray<float> HallLitUntil;

	/** Server -> everyone: a noise at At (its radius cm and how bright it lights the crystals round it). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastNoise(FVector_NetQuantize At, float Radius, float Strength);

	/** Is this hall's keystone light on (every machine). */
	bool IsHallLit(int32 Hall) const;
};

/**
 * A place in the caves you press E at: a hall's keystone chime (Kind keystone: strike it, the hall lights for 20 game
 * minutes), a keepsake (Kind keepsake: a lore item, once each), a find (Kind find: a cache of things the Wardcarvers
 * left, once each). Server-owned; what it gives and who has had it live on the server.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCrystalSpot : public AActor
{
	GENERATED_BODY()

public:
	AWRCrystalSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName SpotId;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	/** A keystone's hall (FWRCrystalData::Halls index). */
	UPROPERTY(Replicated) int32 Hall = -1;
	float Reach = 280.f;

	// Server.
	TMap<FName, int32> Gives;
	FString Lore;
	TSet<FString> DoneBy;
	/** Players who have struck this keystone (Saga's "Listen first" counts halls, not strikes). */
	TSet<FString> StruckBy;

	/** Server: E pressed here. What the player is told. */
	FString Use(AWRPlayerController* PC);
	/** Server: any hit or press strikes a keystone chime (a blow, an arrow, a spell: crystal 2.1). */
	void Strike(AWRCharacterBase* By);

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Glint;
};
