#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCombatTypes.h"
#include "WRProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Server-spawned, replicated spell projectile. Carries a prepared hit and resolves it on impact. */
UCLASS()
class WILDREACH_API AWRProjectile : public AActor
{
	GENERATED_BODY()

public:
	AWRProjectile();

	void Init(const FWRHit& InHit, float Speed, const FLinearColor& Color);
	/** Server: curve toward a locked-on target as it flies (Acceleration cm/s/s; 0 = straight on). */
	void HomeOn(AActor* Target, float Acceleration);
	/** A thrown or shot weapon: flies with drop as a model (MeshAlong = the model's tip axis, empty = from its bounds).
	 *  Kind "bola" tangles what it hits; arrows, bolts, darts and spears stick where they land, and PickupItem (a thrown
	 *  spear) is left on the ground there to pick up again. */
	void InitPhysical(const FWRHit& InHit, const FVector& Velocity, float Gravity, const FString& MeshPath, const FString& MeshAlong, FName InKind, FName InPickupItem);
	/** Server: a thing a boss throws (slag, a boulder, snow, a fire bolt): it arcs under Gravity and bursts where it
	 *  lands, hitting everyone in SplashRadius (cooks/build-combat-full.md 2.16). It never homes. MeshPath: a model
	 *  (a boulder); empty: the element's trail. OnLanded runs on the server where it came down (lava cracks...). */
	void InitLob(const FWRHit& InHit, const FVector& Velocity, float Gravity, float InSplashRadius, const FString& MeshPath);
	TFunction<void(const FVector&)> OnLanded;
	/** Velocity that carries a lob from From to To at HorizontalSpeed under Gravity (x 980 cm/s2). */
	static FVector LobVelocity(const FVector& From, const FVector& To, float HorizontalSpeed, float Gravity);
	/** Server: a bola tangles small creatures' legs (rooted) and slows big ones. */
	static void ApplyBola(class AWRCharacterBase* Target);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult);
	UFUNCTION() void OnRep_Color();
	UFUNCTION() void OnRep_Model();
	UFUNCTION() void OnRep_Fx();
	UFUNCTION() void OnRep_Homing();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing=OnRep_Color) FLinearColor TintColor = FLinearColor::White;
	UPROPERTY(ReplicatedUsing=OnRep_Model) FString ModelPath;
	UPROPERTY(ReplicatedUsing=OnRep_Model) FString ModelAlong;
	/** A spell's look in flight (WRFx: fire.bolt, frost.shard), picked from its element; the grey sphere hides under it. */
	UPROPERTY(ReplicatedUsing=OnRep_Fx) FName TrailFx;
	/** Replicated so clients fly the same curve between the server's corrections. */
	UPROPERTY(ReplicatedUsing=OnRep_Homing) TObjectPtr<AActor> HomingOn;
	UPROPERTY(ReplicatedUsing=OnRep_Homing) float HomingAcceleration = 0.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Trail;
	FName Kind;
	FName PickupItem;

	FWRHit Hit;
	bool bSpent = false;
	float SplashRadius = 0.f;
};
