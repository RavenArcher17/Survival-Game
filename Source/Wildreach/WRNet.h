#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRNet.generated.h"

class UNetDriver;

/**
 * Replication at 100 players (cooks/build-wild-100players.md 3.3-3.5).
 *
 * Budgets: tuning `net.classes` holds a row per class name (WRBuildPiece, WRCreature...): `hz` (NetUpdateFrequency),
 * `minHz`, `cullM` (NetCullDistance), `priority` (NetPriority), `doze` (dormant until something about it changes). A
 * row matches the class or any subclass; the nearest one wins. A field left out keeps the class's own value, and a value
 * an instance set in code (different from its class default) is never overridden.
 *
 * Dozing: those actors go DORM_DormantAll; the server's watcher (UWRNetSubsystem) keeps a shadow of their replicated
 * fields and flushes the dormancy when any of them changes (every one is looked at each net.doze.checkSeconds), so none
 * of the code that writes them has to remember to. Wake() sends a change at once (a door, a blow).
 */
namespace WRNet
{
	struct FBudget
	{
		float Hz = -1.f;
		float MinHz = -1.f;
		float CullCm = -1.f;
		float Priority = -1.f;
		bool bDoze = false;
	};

	/** The row that applies to this class (cached; cleared when tuning reloads). */
	WILDREACH_API const FBudget& BudgetFor(const UClass* Class);
	/** Server: the row's numbers onto this actor (idempotent), and its dozing. */
	WILDREACH_API void ApplyBudget(AActor* Actor);
	/** The update rate this actor has when nothing slows it (its row's hz, else its class default). */
	WILDREACH_API float BaseRate(const AActor* Actor);
	/** Server: a dozing actor's change goes out now (and the watcher takes it as seen). Harmless on anything else. */
	WILDREACH_API void Wake(AActor* Actor);
	/** Server: a dozing actor that changes every frame for a while (a turret tracking) stays awake until told it may
	 *  doze again. Harmless on actors that don't doze. */
	WILDREACH_API void KeepAwake(AActor* Actor, bool bAwake);
	/** Server: a new cull distance that the Replication Graph also hears about. */
	WILDREACH_API void SetCull(AActor* Actor, float Cm);
	/** Whether this server's game net driver should run the Replication Graph (net.graph.mode, -wrrepgraph=). */
	WILDREACH_API bool WantsGraph(const UNetDriver* Driver, const UWorld* World);
	/** Installs the factory that gives a server's game net driver the Wildreach Replication Graph (module startup). */
	WILDREACH_API void InstallGraphFactory();
}

/**
 * Server: applies the budgets as actors appear, runs the doze watcher, and writes one line a minute on how the
 * replication is holding up (players, actors, dozing, woken, frame time).
 */
UCLASS()
class WILDREACH_API UWRNetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UWRNetSubsystem* Get(const UWorld* World);

	/** Watch this actor (it dozes); called by ApplyBudget for rows with doze. */
	void Doze(AActor* Actor);
	/** The watcher takes the actor's current fields as seen (after a Wake). */
	void Seen(AActor* Actor);
	int32 NumDozing() const { return Dozers.Num(); }
	bool IsDozer(const AActor* Actor) const { return Actor && DozerIndex.Contains(Actor); }

private:
	struct FLayout
	{
		TArray<FProperty*> Props;
		TArray<int32> Offsets;
		int32 Size = 0;
		int32 Align = 8;
	};
	struct FDozer
	{
		TWeakObjectPtr<AActor> Actor;
		const FLayout* Layout = nullptr;
		uint8* Shadow = nullptr;
	};

	const FLayout& LayoutFor(const UClass* Class);
	void Capture(FDozer& D, AActor* A) const;
	bool Changed(const FDozer& D, const AActor* A) const;
	void Free(FDozer& D) const;
	void OnSpawned(AActor* Actor);
	void Watch(float DeltaTime);

	TMap<const UClass*, TUniquePtr<FLayout>> Layouts;
	TArray<FDozer> Dozers;
	TMap<TObjectKey<AActor>, int32> DozerIndex;
	TArray<TWeakObjectPtr<AActor>> Pending;
	FDelegateHandle SpawnHandle;
	int32 Cursor = 0;
	double Budget = 0.0;
	int32 WokeThisMinute = 0;
	double NextLogAt = 0.0;
	float FrameMsAvg = 0.f;
	bool bServer = false;
};
