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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult);
	UFUNCTION() void OnRep_Color();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing=OnRep_Color) FLinearColor TintColor = FLinearColor::White;

	FWRHit Hit;
	bool bSpent = false;
};
