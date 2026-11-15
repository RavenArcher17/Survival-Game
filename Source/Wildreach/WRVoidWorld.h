// The Corrupted wasteland's world things (cooks/biome-void.md 2.6-2.7, 2.11, 2.20; cooks/build-biome-void.md 3b): where
// its gathering is (greywood stands, grey salt pans, ashen grain, the Quire's mandrakes, the meteor bowl's star iron, the
// Glass Sea's voidglass: Data/lands/wasteland.json "resources"), the keepsakes at its story sites (the lore items, picked
// up with E), the thrown things that tell you about it (a throwing stone floats in a rift zone and cracks hollow crust;
// an anchor salt bag bursts in a cloud the bosses and the Rift Storm listen for), and the quest deeds Maren Vey counts.
// Server authority: nodes, pickups and landings are the server's; the keepsakes' look is built on every machine from data.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRVoidWorld.generated.h"

class AWRCharacterBase;
class AWRLand;
class AWRPlayerController;
class AWRResourceNode;
class UProceduralMeshComponent;

/** An anchor salt bag burst at At (radius cm) thrown by By: brood rifts close, rift hearts take 300, island pieces hold. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FWRVoidSalted, const FVector& /*At*/, float /*Radius*/, AWRCharacterBase* /*By*/);

namespace WRVoidWorld
{
	/** Server, once the land's places exist: its resource nodes. Every machine: the keepsakes' look. */
	WILDREACH_API void LandReady(AWRLand* Land);
	/** Server, from AWRResourceNode::Harvest: ring voidglass counts for Maren's quest; a pulled mandrake screams. */
	WILDREACH_API void Harvested(AWRResourceNode* Node, AWRCharacterBase* Gatherer, bool bLastHit);
	/** The Wasteland's thrown things (items.json ranged kind): riftstone, anchorsalt. */
	WILDREACH_API bool IsThrownKind(FName Kind);
	/** Every machine: the thrown thing's model (code:riftstone, code:anchorsalt). */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh);
	/** Server: one landed where it fell (the projectile is destroyed by the caller). */
	WILDREACH_API void ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By);
	/** Bound by the systems that answer anchor salt (WRVoidBosses, the Rift Storm). */
	WILDREACH_API FWRVoidSalted& OnSalted();
	/** Server: a deed of Maren's quests (stone_floated, voidglass_ring, choir_road, last_note...) for the player Who is. */
	WILDREACH_API void QuestEvent(AActor* Who, FName Event, int32 Count = 1);
	/** The E prompt for a keepsake within reach (WRLandThings::Prompt asks). */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** E pressed (client): true when a keepsake took it. */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server: land keep (take the keepsake in reach). Empty when the verb isn't ours. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
}

/** A thrown stone come to rest inside a rift zone: it lifts off the ground and bobs (the floating rule at its smallest),
 *  then settles when the zone lets it go. Its height is a pure function of the server clock since it landed. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVoidFloatStone : public AActor
{
	GENERATED_BODY()

public:
	AWRVoidFloatStone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server clock it landed at; the ground point it rose from. */
	UPROPERTY(Replicated) double LandedAt = 0.0;
	UPROPERTY(Replicated) FVector_NetQuantize Ground;
	/** How high it rises (cm): further from the tear, lower. */
	UPROPERTY(Replicated) float Rise = 30.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Model;
	double Now() const;
};
