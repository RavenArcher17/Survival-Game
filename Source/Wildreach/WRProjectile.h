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
	/** A thrown or shot weapon: flies with drop as a model (MeshAlong = the model's tip axis, empty = from its bounds).
	 *  Kind "bola" tangles what it hits; arrows, bolts, darts and spears stick where they land, and PickupItem (a thrown
	 *  spear) is left on the ground there to pick up again. */
	void InitPhysical(const FWRHit& InHit, const FVector& Velocity, float Gravity, const FString& MeshPath, const FString& MeshAlong, FName InKind, FName InPickupItem);
	/** Server: a bola tangles small creatures' legs (rooted) and slows big ones. */
	static void ApplyBola(class AWRCharacterBase* Target);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult);
	UFUNCTION() void OnRep_Color();
	UFUNCTION() void OnRep_Model();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing=OnRep_Color) FLinearColor TintColor = FLinearColor::White;
	UPROPERTY(ReplicatedUsing=OnRep_Model) FString ModelPath;
	UPROPERTY(ReplicatedUsing=OnRep_Model) FString ModelAlong;
	FName Kind;
	FName PickupItem;

	FWRHit Hit;
	bool bSpent = false;
};
