#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "WRNetGraph.generated.h"

class UWRReplicationGraph;

/**
 * Per connection: the players' own states and the people near them every frame, everyone else's in turn (each far one
 * every net.graph.farPeopleSeconds), and the actors whose roster holds this player (portal folds) wherever they are.
 * (cooks/build-wild-100players.md 3.2: Fortnite's player-state frequency limiter, with the near ones kept at full rate
 * because their health and statuses are on the bars you see.)
 */
UCLASS()
class UWRRepNode_People : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override {}
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override { return false; }
	virtual bool NotifyActorRenamed(const FRenamedReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override { return false; }
	virtual void NotifyResetAllNetworkActors() override {}
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual void TearDown() override;

	UPROPERTY()
	TObjectPtr<UWRReplicationGraph> Graph;

private:
	FActorRepListRefView List;
	/** Roster actors whose cull distance is zeroed on this connection (put back when the player leaves the roster). */
	TSet<TWeakObjectPtr<AActor>> Zeroed;
};

USTRUCT()
struct FWRGraphConnection
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UNetConnection> Connection = nullptr;
	UPROPERTY() TObjectPtr<UReplicationGraphNode_AlwaysRelevant_ForConnection> Owned = nullptr;
	UPROPERTY() TObjectPtr<UWRRepNode_People> People = nullptr;
};

/**
 * Wildreach's Replication Graph (cooks/build-wild-100players.md 3.2), made for a server's game net driver by
 * WRNet::InstallGraphFactory when net.graph.mode / -wrrepgraph= ask for it (dedicated servers by default).
 *   Grid (net.graph.cellM cells): everything spatial. Dozing actors (WRNet budgets with doze) are dormancy-driven
 *     (static while dormant), the rest dynamic.
 *   Always relevant: actors with bAlwaysRelevant on the instance (the game state, world states, Hearths, totems).
 *   Per connection: the viewer and its view target, owner-only actors, and UWRRepNode_People.
 * An actor is routed on the replication frame after it was added, so everything set while it spawned (a deferred
 * InitPiece, BeginPlay's cull distance) is read from the instance. Rate changes made later (OnNetUpdateFrequencyChanged)
 * and cull changes (WRNet::SetCull) reach the graph.
 */
UCLASS(transient)
class UWRReplicationGraph : public UReplicationGraph
{
	GENERATED_BODY()

public:
	virtual void InitForNetDriver(UNetDriver* InNetDriver) override;
	virtual void InitGlobalActorClassSettings() override;
	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;
	virtual void RemoveClientConnection(UNetConnection* NetConnection) override;
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	virtual void BeginDestroy() override;

	/** A new cull distance for one actor, globally and on every connection (WRNet::SetCull). */
	void NoteCull(AActor* Actor, float CullCm);
	/** Actors routed by the graph (for the NET log line). */
	int32 NumRouted() const { return Routes.Num(); }

	/** The people node's lists. */
	TArray<TWeakObjectPtr<AActor>> People;
	TArray<TWeakObjectPtr<AActor>> Rosters;

	UPROPERTY()
	TObjectPtr<UReplicationGraphNode_GridSpatialization2D> GridNode;
	UPROPERTY()
	TObjectPtr<UReplicationGraphNode_ActorList> AlwaysNode;
	UPROPERTY()
	TArray<FWRGraphConnection> PerConnection;

private:
	enum class ERoute : uint8 { Dynamic, Dozing, Always, OwnerWaiting, Owned, People, Roster };

	void Settle(AActor* Actor);
	void Unroute(AActor* Actor, ERoute Route);
	void OnRateChanged(const AActor* Actor);
	FWRGraphConnection* ConnectionFor(UNetConnection* Connection);

	/** Added, waiting for the next replication frame to be routed. */
	TArray<TWeakObjectPtr<AActor>> Pending;
	/** Owner-only actors still without a connection. */
	TArray<TWeakObjectPtr<AActor>> OwnerWaiting;
	TMap<TObjectKey<AActor>, ERoute> Routes;
	FDelegateHandle RateHandle;
};
