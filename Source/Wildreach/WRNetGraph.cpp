#include "WRNetGraph.h"
#include "WRNet.h"
#include "WRPortals.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Engine/ChildConnection.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- the people node

void UWRRepNode_People::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	List.Reset();
	if (!Graph)
	{
		return;
	}
	// Who's looking: their own state, their name (rosters) and where they look from.
	const APlayerState* Mine = nullptr;
	FString MyName;
	TArray<FVector, TInlineAllocator<2>> From;
	for (const FNetViewer& V : Params.Viewers)
	{
		if (const APlayerController* PC = Cast<APlayerController>(V.InViewer))
		{
			if (PC->PlayerState)
			{
				Mine = PC->PlayerState;
				MyName = Mine->GetPlayerName();
			}
		}
		From.Add(V.ViewLocation);
	}
	const float NearSq = FMath::Square(WRTune(TEXT("net.graph.peopleNearM"), 150.f) * 100.f);
	const float TickRate = Graph->NetDriver ? float(Graph->NetDriver->GetNetServerMaxTickRate()) : 30.f;
	const uint32 Spread = uint32(FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("net.graph.farPeopleSeconds"), 2.f) * TickRate)));
	const uint32 Frame = Params.ReplicationFrameNum;
	for (int32 i = 0; i < Graph->People.Num(); ++i)
	{
		AActor* A = Graph->People[i].Get();
		const APlayerState* PS = Cast<APlayerState>(A);
		if (!PS)
		{
			continue;
		}
		bool bWant = PS == Mine;
		if (!bWant)
		{
			if (const APawn* Body = PS->GetPawn())
			{
				const FVector At = Body->GetActorLocation();
				for (const FVector& F : From)
				{
					if (FVector::DistSquared(F, At) <= NearSq) { bWant = true; break; }
				}
			}
		}
		// The far ones in turn, each on its own frame of the spread (not all on the same one).
		if (!bWant)
		{
			bWant = (Frame + uint32(i)) % Spread == 0;
		}
		if (bWant)
		{
			List.ConditionalAdd(A);
		}
	}
	// Rosters (a portal fold's party): gathered for its members wherever they are, with the distance cull off for them
	// only; anyone else sees it through the grid like any other actor.
	for (const TWeakObjectPtr<AActor>& Weak : Graph->Rosters)
	{
		AActor* A = Weak.Get();
		const AWRFold* Fold = Cast<AWRFold>(A);
		if (!Fold)
		{
			continue;
		}
		const bool bMember = !MyName.IsEmpty() && Fold->Roster.Contains(MyName);
		if (bMember)
		{
			if (!Zeroed.Contains(Weak))
			{
				Params.ConnectionManager.ActorInfoMap.FindOrAdd(A).SetCullDistanceSquared(0.f);
				Zeroed.Add(Weak);
			}
			List.ConditionalAdd(A);
		}
		else if (Zeroed.Remove(Weak) > 0)
		{
			Params.ConnectionManager.ActorInfoMap.FindOrAdd(A).SetCullDistanceSquared(A->GetNetCullDistanceSquared());
		}
	}
	for (auto It = Zeroed.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}
	if (List.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(List);
	}
}

void UWRRepNode_People::TearDown()
{
	Super::TearDown();
	List.TearDown();
	Zeroed.Reset();
}

// ---------------------------------------------------------------- the graph

void UWRReplicationGraph::InitForNetDriver(UNetDriver* InNetDriver)
{
	Super::InitForNetDriver(InNetDriver);
	if (InNetDriver)
	{
		RateHandle = InNetDriver->GetOnNetUpdateFrequencyChanged().AddUObject(this, &UWRReplicationGraph::OnRateChanged);
	}
	UE_LOG(LogWildreach, Log, TEXT("NET replication graph on %s (cells %.0f m, near people %.0f m, far people every %.1f s, tick %d Hz)"),
		*GetNameSafe(InNetDriver), WRTune(TEXT("net.graph.cellM"), 100.f), WRTune(TEXT("net.graph.peopleNearM"), 150.f),
		WRTune(TEXT("net.graph.farPeopleSeconds"), 2.f), InNetDriver ? InNetDriver->GetNetServerMaxTickRate() : 0);
}

void UWRReplicationGraph::BeginDestroy()
{
	if (NetDriver && RateHandle.IsValid())
	{
		NetDriver->GetOnNetUpdateFrequencyChanged().Remove(RateHandle);
	}
	RateHandle.Reset();
	Super::BeginDestroy();
}

void UWRReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();
	// The class defaults, as the engine's basic graph takes them; each actor's own numbers replace them when it settles.
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		const AActor* CDO = Cast<AActor>(Class->GetDefaultObject(false));
		if (!CDO || !CDO->GetIsReplicated() || Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		FClassReplicationInfo Info;
		const WRNet::FBudget& B = WRNet::BudgetFor(Class);
		const float Hz = B.Hz > 0.f ? B.Hz : CDO->GetNetUpdateFrequency();
		Info.ReplicationPeriodFrame = GetReplicationPeriodFrameForFrequency(FMath::Max(0.1f, Hz));
		Info.SetCullDistanceSquared(CDO->bAlwaysRelevant || CDO->bOnlyRelevantToOwner ? 0.f : (B.CullCm > 0.f ? FMath::Square(B.CullCm) : CDO->GetNetCullDistanceSquared()));
		GlobalActorReplicationInfoMap.SetClassInfo(Class, Info);
	}
}

void UWRReplicationGraph::InitGlobalGraphNodes()
{
	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = FMath::Max(20.f, WRTune(TEXT("net.graph.cellM"), 100.f)) * 100.f;
	const double Bias = double(FMath::Max(1000.f, WRTune(TEXT("net.graph.biasM"), 60000.f))) * 100.0;
	GridNode->SpatialBias = FVector2D(-Bias, -Bias);
	AddGlobalGraphNode(GridNode);

	AlwaysNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysNode);
}

void UWRReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);
	FWRGraphConnection Entry;
	Entry.Connection = RepGraphConnection->NetConnection;
	Entry.Owned = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
	AddConnectionGraphNode(Entry.Owned, RepGraphConnection);
	Entry.People = CreateNewNode<UWRRepNode_People>();
	Entry.People->Graph = this;
	AddConnectionGraphNode(Entry.People, RepGraphConnection);
	PerConnection.Add(Entry);
}

void UWRReplicationGraph::RemoveClientConnection(UNetConnection* NetConnection)
{
	PerConnection.RemoveAll([NetConnection](const FWRGraphConnection& E) { return E.Connection == NetConnection || !E.Connection; });
	Super::RemoveClientConnection(NetConnection);
}

FWRGraphConnection* UWRReplicationGraph::ConnectionFor(UNetConnection* Connection)
{
	if (UChildConnection* Child = Connection ? Connection->GetUChildConnection() : nullptr)
	{
		Connection = Child->Parent;
	}
	if (!Connection)
	{
		return nullptr;
	}
	return PerConnection.FindByPredicate([Connection](const FWRGraphConnection& E) { return E.Connection == Connection; });
}

void UWRReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	// Routed when the next replication frame starts: by then whatever its spawn set (a deferred InitPiece's
	// bAlwaysRelevant, BeginPlay's cull distance) is on the instance.
	if (AActor* A = ActorInfo.GetActor())
	{
		Pending.AddUnique(A);
	}
}

void UWRReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* A = ActorInfo.GetActor();
	if (!A)
	{
		return;
	}
	Pending.Remove(A);
	if (const ERoute* Route = Routes.Find(A))
	{
		Unroute(A, *Route);
		Routes.Remove(A);
	}
}

void UWRReplicationGraph::Settle(AActor* A)
{
	WRNet::ApplyBudget(A);
	FGlobalActorReplicationInfo& G = GlobalActorReplicationInfoMap.Get(A);
	const float Hz = A->GetNetUpdateFrequency();
	if (Hz > 0.f)
	{
		G.Settings.ReplicationPeriodFrame = GetReplicationPeriodFrameForFrequency(Hz);
	}
	const FNewReplicatedActorInfo Info(A);
	if (A->IsA<APlayerState>())
	{
		G.Settings.SetCullDistanceSquared(0.f);
		People.Add(A);
		Routes.Add(A, ERoute::People);
		return;
	}
	if (A->bAlwaysRelevant)
	{
		G.Settings.SetCullDistanceSquared(0.f);
		AlwaysNode->NotifyAddNetworkActor(Info);
		Routes.Add(A, ERoute::Always);
		return;
	}
	if (A->bOnlyRelevantToOwner)
	{
		G.Settings.SetCullDistanceSquared(0.f);
		if (FWRGraphConnection* C = ConnectionFor(A->GetNetConnection()))
		{
			C->Owned->NotifyAddNetworkActor(Info);
			Routes.Add(A, ERoute::Owned);
		}
		else
		{
			OwnerWaiting.Add(A);
			Routes.Add(A, ERoute::OwnerWaiting);
		}
		return;
	}
	G.Settings.SetCullDistanceSquared(A->GetNetCullDistanceSquared());
	if (A->IsA<AWRFold>())
	{
		Rosters.Add(A);
		GridNode->AddActor_Dynamic(Info, G);
		Routes.Add(A, ERoute::Roster);
		return;
	}
	if (WRNet::BudgetFor(A->GetClass()).bDoze)
	{
		// Static in its cells while it dozes, dynamic for the moments it's awake.
		GridNode->AddActor_Dormancy(Info, G);
		Routes.Add(A, ERoute::Dozing);
		return;
	}
	GridNode->AddActor_Dynamic(Info, G);
	Routes.Add(A, ERoute::Dynamic);
}

void UWRReplicationGraph::Unroute(AActor* A, ERoute Route)
{
	const FNewReplicatedActorInfo Info(A);
	switch (Route)
	{
	case ERoute::Dynamic:
		GridNode->RemoveActor_Dynamic(Info);
		break;
	case ERoute::Roster:
		GridNode->RemoveActor_Dynamic(Info);
		Rosters.Remove(A);
		break;
	case ERoute::Dozing:
		GridNode->RemoveActor_Dormancy(Info);
		break;
	case ERoute::Always:
		AlwaysNode->NotifyRemoveNetworkActor(Info);
		SetActorDestructionInfoToIgnoreDistanceCulling(A);
		break;
	case ERoute::People:
		People.Remove(A);
		SetActorDestructionInfoToIgnoreDistanceCulling(A);
		break;
	case ERoute::OwnerWaiting:
		OwnerWaiting.Remove(A);
		break;
	case ERoute::Owned:
		// Its connection may already be gone: every connection's list lets go of it.
		for (FWRGraphConnection& C : PerConnection)
		{
			if (C.Owned) C.Owned->NotifyRemoveNetworkActor(Info, false);
		}
		break;
	}
}

int32 UWRReplicationGraph::ServerReplicateActors(float DeltaSeconds)
{
	if (Pending.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Now = MoveTemp(Pending);
		Pending.Reset();
		for (const TWeakObjectPtr<AActor>& Weak : Now)
		{
			AActor* A = Weak.Get();
			if (A && !Routes.Contains(A) && IsActorValidForReplicationGather(A))
			{
				Settle(A);
			}
		}
	}
	for (int32 i = OwnerWaiting.Num() - 1; i >= 0; --i)
	{
		AActor* A = OwnerWaiting[i].Get();
		if (!A)
		{
			OwnerWaiting.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		if (FWRGraphConnection* C = ConnectionFor(A->GetNetConnection()))
		{
			C->Owned->NotifyAddNetworkActor(FNewReplicatedActorInfo(A));
			Routes.Add(A, ERoute::Owned);
			OwnerWaiting.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}
	People.RemoveAll([](const TWeakObjectPtr<AActor>& W) { return !W.IsValid(); });
	Rosters.RemoveAll([](const TWeakObjectPtr<AActor>& W) { return !W.IsValid(); });
	return Super::ServerReplicateActors(DeltaSeconds);
}

void UWRReplicationGraph::OnRateChanged(const AActor* ConstActor)
{
	AActor* A = const_cast<AActor*>(ConstActor);
	if (!A || !Routes.Contains(A) || A->GetNetUpdateFrequency() <= 0.f)
	{
		return;   // not routed yet: it settles with the new rate
	}
	FGlobalActorReplicationInfo* G = GlobalActorReplicationInfoMap.Find(A);
	if (!G)
	{
		return;
	}
	const uint16 Period = GetReplicationPeriodFrameForFrequency(A->GetNetUpdateFrequency());
	G->Settings.ReplicationPeriodFrame = Period;
	for (UNetReplicationGraphConnection* C : Connections)
	{
		if (FConnectionReplicationActorInfo* Info = C ? C->ActorInfoMap.Find(A) : nullptr)
		{
			Info->ReplicationPeriodFrame = Period;
		}
	}
}

void UWRReplicationGraph::NoteCull(AActor* A, float CullCm)
{
	if (const ERoute* Route = A ? Routes.Find(A) : nullptr)
	{
		if (*Route == ERoute::Dynamic || *Route == ERoute::Dozing || *Route == ERoute::Roster)
		{
			SetAllCullDistanceSettingsForActor(A, FMath::Square(CullCm));
		}
	}
}
