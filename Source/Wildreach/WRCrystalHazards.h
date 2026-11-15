#pragma once

// The Crystal caves' hazards and machines (cooks/biome-crystal.md 2.5, 2.8; cooks/build-biome-crystal.md 2.8-2.10).
//
// Ward fields: the Wardcarvers' barriers across doorways, a sheet you can't see but for a faint violet shimmer at its
// edges. An unattuned character near it sees the tell (the floor's crystal dust jumps into Chladni lines along it, the
// hexafoil over the door glows, a hum rises over 0.9 s); one who crosses takes 25 arcane through ApplyHit and is thrown
// 6 m back the way they came, and the field can't throw them again for 1.5 s. Attuned (the Arcane path, the ward charm,
// Wardcloth's four, Venn's ring) walk through with a soft chime; attuning chalk halves it and stops the throw. A lead
// keystone plate in sight of each field drops it for 90 s when a projectile strikes it. Pattern A is live normally, B in
// the Resonance.
//
// Shard chandeliers: a crystal mass on a bronze chain to an iron ring leaded into the ceiling; a loud noise under one
// makes it shiver 1 s and drop (a gravity curve on the server's clock: every machine draws the same fall), 70 damage in
// 3 m; it re-hangs after 2 game days, drawn up over 20 s. Thin geode floors: a crust over a cup, hollow underfoot; a roll,
// a landing or a heavy creature breaks a 4 m hole. Scalding pools: wading burns. The Silent Vault's door: stand on its
// hexafoil and make no sound for 10 s and its six petals go dark one by one; at the sixth it rolls aside on its rail.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCrystalHazards.generated.h"

class AWRCharacterBase;
class AWRPlayerState;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UBoxComponent;
class UAudioComponent;
class UPointLightComponent;

UCLASS(NotPlaceable)
class WILDREACH_API AWRWardField : public AActor
{
	GENERATED_BODY()

public:
	AWRWardField();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Its row in FWRCrystalData::Wards. */
	UPROPERTY(Replicated) int32 Ward = -1;
	/** Server time it comes back up after a keystone or a ward fork (0: up). */
	UPROPERTY(Replicated) float DownUntil = 0.f;
	/** Its pattern has it live now (A normally, B in the Resonance). */
	UPROPERTY(Replicated) bool bPattern = true;

	bool IsLive() const;
	/** Server: drop it for Seconds (a keystone struck, a ward fork). By: who did it (Saga's "Drop the door"). */
	void Drop(float Seconds, AActor* By, bool bKeystone);
	/** Attuned: walks through it unharmed (crystal 2.8). bHalf: attuning chalk (half damage, no throw). */
	static bool IsAttuned(const AActor* Who, bool& bHalf);

private:
	void BuildLook();
	void StepServer();
	float Tell = 0.f;
	float ServerClock = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> Side;
	TMap<TWeakObjectPtr<AActor>, float> LastThrow;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Dust;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Frame;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DustGlow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrameGlow;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
};

/** A ward's lead keystone plate (projectiles strike it: its box blocks them). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWardKeystone : public AActor
{
	GENERATED_BODY()

public:
	AWRWardKeystone();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) TObjectPtr<AWRWardField> Field;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Server: a projectile struck it. */
	void Struck(AActor* By);

private:
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Plate;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	UPROPERTY() TObjectPtr<UBoxComponent> Hitbox;
	float Shown = -1.f;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRChandelier : public AActor
{
	GENERATED_BODY()

public:
	AWRChandelier();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 hung, 1 shivering, 2 falling, 3 down (its shards on the floor), 4 being drawn back up. */
	UPROPERTY(Replicated) uint8 Phase = 0;
	/** Server time the phase began. */
	UPROPERTY(Replicated) float PhaseAt = 0.f;
	UPROPERTY(Replicated) float TopZ = 0.f;
	UPROPERTY(Replicated) float FloorZ = 0.f;
	/** Server: a noise (from UWRCrystalWorld::OnNoise). */
	void Heard(const FVector& At, float Radius);
	/** Server: drop it now (the Resonance's peal). */
	void Shake();

private:
	float MassZ(float Now) const;
	void BuildLook();
	void Land();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Mass;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Chain;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Pile;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	uint8 ShownPhase = 255;
	FTimerHandle Rehang;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRThinFloor : public AActor
{
	GENERATED_BODY()

public:
	AWRThinFloor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(ReplicatedUsing=OnRep_Broken) bool bBroken = false;
	UPROPERTY(Replicated) float Radius = 400.f;
	/** Server: a roll, a landing, a heavy blow or a heavy creature's step on it. */
	void Heard(const FVector& At, float NoiseRadius, AActor* By, FName Kind);

private:
	UFUNCTION() void OnRep_Broken();
	void BuildLook();
	void Break();
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Crust;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Rim;
	UPROPERTY() TObjectPtr<UBoxComponent> Floor;
	float BoomClock = 0.f;
	FTimerHandle Reform;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRSilentDoor : public AActor
{
	GENERATED_BODY()

public:
	AWRSilentDoor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Petals still lit (6: none dimmed). */
	UPROPERTY(Replicated) uint8 Petals = 6;
	/** Server time the door began rolling open (0: shut) and began rolling shut. */
	UPROPERTY(Replicated) float OpenAt = 0.f;
	UPROPERTY(Replicated) float CloseAt = 0.f;
	/** Server: a noise. */
	void Heard(const FVector& At, float Radius);
	/** Where the hexafoil lies (world). */
	FVector PadAt() const;

private:
	float Slide(float Now) const;
	void BuildLook();
	float QuietSince = 0.f;
	float NextPetalAt = 0.f;
	uint8 ShownPetals = 255;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Disc;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> PetalGlow;
	UPROPERTY() TObjectPtr<UBoxComponent> Block;
};

namespace WRCrystal
{
	/** Server: a crystal item used from the bag (attuning chalk, a ward fork). Empty: not one of ours. */
	WILDREACH_API FString UseItem(AWRPlayerState* PS, FName Item, int32 Slot);
	/** Server: the Lumen lantern struck awake (it flares 20 m for 5 s and rings 10 m). */
	WILDREACH_API void LanternLit(AWRCharacterBase* Who, FName LightItem);
	/** A spell started in the Silent Vault fizzles (every machine asks; the server shows it). */
	WILDREACH_API bool SpellFails(const AActor* Caster);
}
