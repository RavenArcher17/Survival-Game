#include "WRNet.h"
#include "WRNetGraph.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

namespace WRNetDetail
{
	/** Class -> its row, resolved once per tuning (Reload changes the root object, which clears it). */
	struct FCache
	{
		const FJsonObject* Root = nullptr;
		TMap<const UClass*, WRNet::FBudget> ByClass;
	};
	FCache& Cache()
	{
		static FCache C;
		return C;
	}

	bool IsServer(const UWorld* World)
	{
		const ENetMode Mode = World ? World->GetNetMode() : NM_Standalone;
		return Mode == NM_DedicatedServer || Mode == NM_ListenServer;
	}

	UWRReplicationGraph* GraphOf(const AActor* Actor)
	{
		const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		return Driver ? Cast<UWRReplicationGraph>(Driver->GetReplicationDriver()) : nullptr;
	}
}

const WRNet::FBudget& WRNet::BudgetFor(const UClass* Class)
{
	using namespace WRNetDetail;
	static const FBudget None;
	if (!Class) return None;
	FCache& C = Cache();
	const TSharedPtr<FJsonObject> Rows = FWRTuning::Get().Object(TEXT("net.classes"));
	if (C.Root != Rows.Get())
	{
		C.Root = Rows.Get();
		C.ByClass.Reset();
	}
	if (const FBudget* Found = C.ByClass.Find(Class)) return *Found;
	FBudget B;
	// The nearest class up the chain with a row ("WRBuildPiece" for AWRBuildPiece: the class name without its prefix).
	for (const UClass* K = Class; K && Rows.IsValid(); K = K->GetSuperClass())
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!Rows->TryGetObjectField(K->GetName(), Row) || !Row || !Row->IsValid()) continue;
		double V = 0.0;
		if ((*Row)->TryGetNumberField(TEXT("hz"), V)) B.Hz = float(V);
		if ((*Row)->TryGetNumberField(TEXT("minHz"), V)) B.MinHz = float(V);
		if ((*Row)->TryGetNumberField(TEXT("cullM"), V)) B.CullCm = float(V) * 100.f;
		if ((*Row)->TryGetNumberField(TEXT("priority"), V)) B.Priority = float(V);
		(*Row)->TryGetBoolField(TEXT("doze"), B.bDoze);
		break;
	}
	return C.ByClass.Add(Class, B);
}

float WRNet::BaseRate(const AActor* Actor)
{
	if (!Actor) return 0.f;
	const FBudget& B = BudgetFor(Actor->GetClass());
	return B.Hz > 0.f ? B.Hz : Actor->GetClass()->GetDefaultObject<AActor>()->GetNetUpdateFrequency();
}

void WRNet::ApplyBudget(AActor* A)
{
	if (!A || !A->GetIsReplicated() || !WRNetDetail::IsServer(A->GetWorld()) || A->IsActorBeingDestroyed()) return;
	const FBudget& B = BudgetFor(A->GetClass());
	const AActor* CDO = A->GetClass()->GetDefaultObject<AActor>();
	// Only what the instance hasn't set for itself in code (its value still the class default).
	if (B.Hz > 0.f && FMath::IsNearlyEqual(A->GetNetUpdateFrequency(), CDO->GetNetUpdateFrequency()) && !FMath::IsNearlyEqual(A->GetNetUpdateFrequency(), B.Hz))
	{
		A->SetNetUpdateFrequency(B.Hz);
	}
	if (B.MinHz > 0.f && FMath::IsNearlyEqual(A->GetMinNetUpdateFrequency(), CDO->GetMinNetUpdateFrequency()))
	{
		A->SetMinNetUpdateFrequency(FMath::Min(B.MinHz, A->GetNetUpdateFrequency()));
	}
	if (B.CullCm > 0.f && FMath::IsNearlyEqual(A->GetNetCullDistanceSquared(), CDO->GetNetCullDistanceSquared()))
	{
		A->SetNetCullDistanceSquared(FMath::Square(B.CullCm));
	}
	if (B.Priority > 0.f && FMath::IsNearlyEqual(A->NetPriority, CDO->NetPriority))
	{
		A->NetPriority = B.Priority;
	}
	if (B.bDoze && !A->bAlwaysRelevant)
	{
		if (UWRNetSubsystem* Net = UWRNetSubsystem::Get(A->GetWorld()))
		{
			Net->Doze(A);
		}
	}
}

void WRNet::Wake(AActor* A)
{
	if (!A || !A->HasAuthority() || !A->GetIsReplicated()) return;
	if (A->NetDormancy > DORM_Awake)
	{
		A->FlushNetDormancy();
	}
	A->ForceNetUpdate();
	if (UWRNetSubsystem* Net = UWRNetSubsystem::Get(A->GetWorld()))
	{
		Net->Seen(A);
	}
}

void WRNet::KeepAwake(AActor* A, bool bAwake)
{
	if (!A || !A->HasAuthority()) return;
	UWRNetSubsystem* Net = UWRNetSubsystem::Get(A->GetWorld());
	if (!Net || !Net->IsDozer(A)) return;
	const ENetDormancy Want = bAwake ? DORM_Awake : DORM_DormantAll;
	if (A->NetDormancy == Want) return;
	A->SetNetDormancy(Want);
	if (!bAwake) Net->Seen(A);   // what it last sent awake is what clients have
}

void WRNet::SetCull(AActor* A, float Cm)
{
	if (!A) return;
	A->SetNetCullDistanceSquared(FMath::Square(Cm));
	if (UWRReplicationGraph* Graph = WRNetDetail::GraphOf(A))
	{
		Graph->NoteCull(A, Cm);
	}
}

bool WRNet::WantsGraph(const UNetDriver* Driver, const UWorld* World)
{
	if (!Driver || Driver->NetDriverName != NAME_GameNetDriver || !Driver->IsServer())
	{
		return false;
	}
	// -wrrepgraph=on|off on the server's command line, else tuning net.graph.mode: dedicated (default), always, off.
	FString Mode;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-wrrepgraph="), Mode))
	{
		const TSharedPtr<FJsonObject> Graph = FWRTuning::Get().Object(TEXT("net.graph"));
		if (!Graph.IsValid() || !Graph->TryGetStringField(TEXT("mode"), Mode)) Mode = TEXT("dedicated");
	}
	Mode = Mode.ToLower();
	if (Mode == TEXT("off") || Mode == TEXT("0") || Mode == TEXT("false")) return false;
	if (Mode == TEXT("on") || Mode == TEXT("always") || Mode == TEXT("1") || Mode == TEXT("true")) return true;
	return IsRunningDedicatedServer();
}

void WRNet::InstallGraphFactory()
{
	UReplicationDriver::CreateReplicationDriverDelegate().BindLambda([](UNetDriver* Driver, const FURL& URL, UWorld* World) -> UReplicationDriver*
	{
		// Nothing returned: the engine's own replication (listen servers by default; proven at their 2-16 players).
		return WRNet::WantsGraph(Driver, World) ? NewObject<UWRReplicationGraph>(GetTransientPackage()) : nullptr;
	});
}

// ---------------------------------------------------------------- the subsystem

bool UWRNetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

UWRNetSubsystem* UWRNetSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UWRNetSubsystem>() : nullptr;
}

TStatId UWRNetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRNetSubsystem, STATGROUP_Tickables);
}

void UWRNetSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bServer = WRNetDetail::IsServer(&InWorld);
	if (!bServer)
	{
		return;
	}
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UWRNetSubsystem::OnSpawned));
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->GetIsReplicated()) Pending.Add(*It);
	}
	NextLogAt = InWorld.GetTimeSeconds() + 60.0;
}

void UWRNetSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld(); World && SpawnHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	for (FDozer& D : Dozers) Free(D);
	Dozers.Reset();
	DozerIndex.Reset();
	Super::Deinitialize();
}

void UWRNetSubsystem::OnSpawned(AActor* Actor)
{
	if (Actor && Actor->GetIsReplicated()) Pending.Add(Actor);
}

const UWRNetSubsystem::FLayout& UWRNetSubsystem::LayoutFor(const UClass* Class)
{
	if (const TUniquePtr<FLayout>* Found = Layouts.Find(Class)) return **Found;
	TUniquePtr<FLayout> L = MakeUnique<FLayout>();
	// Every replicated field of the class (its own and its parents'), packed into one block per actor.
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* P = *It;
		if (!P->HasAnyPropertyFlags(CPF_Net)) continue;
		const int32 Align = FMath::Max(1, P->GetMinAlignment());
		const int32 Offset = Align > 1 ? int32(Align * ((L->Size + Align - 1) / Align)) : L->Size;
		L->Props.Add(P);
		L->Offsets.Add(Offset);
		L->Size = Offset + P->GetSize();
		L->Align = FMath::Max(L->Align, Align);
	}
	return *Layouts.Add(Class, MoveTemp(L));
}

void UWRNetSubsystem::Capture(FDozer& D, AActor* A) const
{
	const FLayout& L = *D.Layout;
	if (!D.Shadow)
	{
		D.Shadow = static_cast<uint8*>(FMemory::Malloc(FMath::Max(1, L.Size), L.Align));
		for (int32 i = 0; i < L.Props.Num(); ++i) L.Props[i]->InitializeValue(D.Shadow + L.Offsets[i]);
	}
	for (int32 i = 0; i < L.Props.Num(); ++i)
	{
		L.Props[i]->CopyCompleteValue(D.Shadow + L.Offsets[i], L.Props[i]->ContainerPtrToValuePtr<void>(A));
	}
}

bool UWRNetSubsystem::Changed(const FDozer& D, const AActor* A) const
{
	const FLayout& L = *D.Layout;
	for (int32 i = 0; i < L.Props.Num(); ++i)
	{
		const FProperty* P = L.Props[i];
		const int32 Elem = FMath::Max(1, P->GetElementSize());
		const int32 Dim = FMath::Max(1, P->GetSize() / Elem);
		for (int32 k = 0; k < Dim; ++k)
		{
			if (!P->Identical(D.Shadow + L.Offsets[i] + Elem * k, P->ContainerPtrToValuePtr<void>(A, k), 0)) return true;
		}
	}
	return false;
}

void UWRNetSubsystem::Free(FDozer& D) const
{
	if (!D.Shadow || !D.Layout) return;
	for (int32 i = 0; i < D.Layout->Props.Num(); ++i) D.Layout->Props[i]->DestroyValue(D.Shadow + D.Layout->Offsets[i]);
	FMemory::Free(D.Shadow);
	D.Shadow = nullptr;
}

void UWRNetSubsystem::Doze(AActor* A)
{
	if (!A || DozerIndex.Contains(A)) return;
	FDozer D;
	D.Actor = A;
	D.Layout = &LayoutFor(A->GetClass());
	Capture(D, A);
	DozerIndex.Add(A, Dozers.Add(D));
	// Sent once as it is now (to whoever it's relevant to), then quiet until it changes.
	A->SetNetDormancy(DORM_DormantAll);
}

void UWRNetSubsystem::Seen(AActor* A)
{
	if (const int32* Index = A ? DozerIndex.Find(A) : nullptr)
	{
		Capture(Dozers[*Index], A);
	}
}

void UWRNetSubsystem::Watch(float DeltaTime)
{
	if (Dozers.Num() == 0) return;
	// Each dozing actor once every checkSeconds, spread evenly over the frames, at most perFrame a frame.
	const float Every = FMath::Max(0.05f, WRTune(TEXT("net.doze.checkSeconds"), 0.25f));
	Budget += double(Dozers.Num()) * DeltaTime / Every;
	const int32 Cap = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("net.doze.perFrame"), 600.f)));
	int32 Count = FMath::Min(Cap, FMath::FloorToInt(Budget));
	Budget = FMath::Min(Budget - Count, double(Cap));
	while (Count-- > 0 && Dozers.Num() > 0)
	{
		if (Cursor >= Dozers.Num()) Cursor = 0;
		FDozer& D = Dozers[Cursor];
		AActor* A = D.Actor.Get();
		if (!A || A->IsActorBeingDestroyed())
		{
			// Gone: its place goes to the last one.
			Free(D);
			for (auto It = DozerIndex.CreateIterator(); It; ++It)
			{
				if (It->Value == Cursor) { It.RemoveCurrent(); break; }
			}
			const int32 Last = Dozers.Num() - 1;
			if (Cursor != Last)
			{
				Dozers[Cursor] = Dozers[Last];
				if (AActor* Moved = Dozers[Cursor].Actor.Get()) DozerIndex.Add(Moved, Cursor);
			}
			Dozers.RemoveAt(Last, 1, EAllowShrinking::No);
			continue;
		}
		if (A->NetDormancy == DORM_Awake)
		{
			++Cursor;   // kept awake (KeepAwake): it replicates as it changes; nothing to wake
			continue;
		}
		if (Changed(D, A))
		{
			A->FlushNetDormancy();
			Capture(D, A);
			++WokeThisMinute;
		}
		++Cursor;
	}
}

void UWRNetSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bServer)
	{
		return;
	}
	if (Pending.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Now = MoveTemp(Pending);
		Pending.Reset();
		for (const TWeakObjectPtr<AActor>& Weak : Now)
		{
			WRNet::ApplyBudget(Weak.Get());
		}
	}
	Watch(DeltaTime);
	FrameMsAvg = FMath::Lerp(FrameMsAvg, DeltaTime * 1000.f, 0.05f);
	UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() >= NextLogAt)
	{
		NextLogAt = World->GetTimeSeconds() + 60.0;
		const AGameStateBase* GS = World->GetGameState();
		const UNetDriver* Driver = World->GetNetDriver();
		const UWRReplicationGraph* Graph = Driver ? Cast<UWRReplicationGraph>(Driver->GetReplicationDriver()) : nullptr;
		UE_LOG(LogWildreach, Log, TEXT("NET %d players, %d clients, %s, %d dozing (%d woke this minute), frame %.1f ms"),
			GS ? GS->PlayerArray.Num() : 0, Driver ? Driver->ClientConnections.Num() : 0,
			Graph ? *FString::Printf(TEXT("graph routes %d actors"), Graph->NumRouted()) : TEXT("engine replication"),
			Dozers.Num(), WokeThisMinute, FrameMsAvg);
		WokeThisMinute = 0;
	}
}
