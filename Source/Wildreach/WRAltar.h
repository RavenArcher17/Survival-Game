#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAltar.generated.h"

class AWRPlayerController;
class UStaticMeshComponent;

/**
 * Boss altar inside a major boss's arena. Interacting takes the altar's path as your subclass
 * (or swaps it in; progress is saved per path). Access is gated by the arena itself: the boss
 * stands between you and the altar, and bosses respawn.
 */
UCLASS()
class WILDREACH_API AWRBossAltar : public AActor
{
	GENERATED_BODY()

public:
	AWRBossAltar();

	/** Path this altar grants ("fire" in the volcanic arena, "frost" in the tundra arena). */
	UPROPERTY(EditAnywhere, Replicated, Category="Altar") FName PathId = TEXT("frost");

	static constexpr float InteractRange = 300.f;

	/** Server: run the altar for this player. Returns a message for the player. */
	FString Interact(AWRPlayerController* Player);

	/** What the prompt says for this player (client). */
	FString GetPrompt(const AWRPlayerController* Player) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pedestal;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Flame;
};
