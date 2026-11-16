#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRPerf.generated.h"

class AActor;
class SWidget;
class UMaterialInterface;

/**
 * Performance on the player's own machine (cooks/cookpass-performance.md; numbers in Data/tuning.json "perf"). Client
 * only: nothing here runs on a dedicated server or a headless (-nullrhi) bot, and nothing is replicated.
 *  - Graphics memory (DXGI: this process's use and the budget Windows gives it), shown on the Video page and the frame
 *    time readout.
 *  - The texture streaming pool sized to the card and the texture level (r.Streaming.PoolSize), not the engine's fixed
 *    600 MB at Medium.
 *  - The first launch picks the quality for this computer (the hardware benchmark, then caps by graphics memory), once.
 *  - Shader (PSO) compiling: fast and boosted while the loading cover is black, in the background in play; the cover
 *    waits for it (Ready / Stage, read by WRUIFlow).
 *  - The frame time readout (Video page switch, wr.perfoverlay).
 */
namespace WRPerf
{
	struct FMemory
	{
		/** This process's graphics memory in use, and the budget Windows gives it (GB; < 0: unknown). */
		double UsedGB = -1.0;
		double BudgetGB = -1.0;
		bool IsKnown() const { return BudgetGB > 0.0 && UsedGB >= 0.0; }
	};
	/** Graphics memory now (refreshed at most every quarter second). */
	WILDREACH_API FMemory Memory();
	/** The card's own memory (GB; < 0: unknown, e.g. a headless run). */
	WILDREACH_API double CardGB();

	/** The hardware benchmark, then the graphics-memory caps (perf.vramTiers), applied and saved. Returns a line for the player. */
	WILDREACH_API FText AutoPick();
	/** The first launch's note ("Graphics set for this computer: ..."), once; empty after it's taken or if there was none. */
	WILDREACH_API FText TakeFirstRunNote();

	/** Shader pipeline states still compiling (PSO precache requests + the bundled/user cache's precompiles). */
	WILDREACH_API int32 ShadersPending();
	/** The preload is in, the warm-up has drawn, and no shaders are compiling: safe to lift the loading cover. */
	WILDREACH_API bool Ready(const UWorld* World);
	/** What the loading cover waits for, given whether the island's cells are in. */
	WILDREACH_API FText Stage(const UWorld* World, bool bIslandIn);
	/** The loading cover is black (true) or play is showing (false): compile fast or in the background. */
	WILDREACH_API void NoteCovered(bool bCovered);
	/** True while some local cover is black (the warm-up only draws then). */
	WILDREACH_API bool IsCovered();

	/** The frame time readout. */
	WILDREACH_API bool OverlayOn();
	WILDREACH_API void SetOverlay(bool bOn);
}

/** The machine-wide part: memory, the texture pool, the first launch's pick, batching, the readout. */
UCLASS()
class WILDREACH_API UWRPerfSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaTime);
	void TickPool();
	void TickFirstRun();
	void TickOverlay();

	FTSTicker::FDelegateHandle TickHandle;
	int32 PoolLevel = -1;
	double PoolCardGB = -2.0;
	double NextPoolCheck = 0.0;
	bool bFirstRunDone = false;
	TSharedPtr<SWidget> Overlay;
	/** The world the readout was added under (a map change clears the viewport's widgets: add it again). */
	TWeakObjectPtr<UWorld> OverlayWorld;
};

/**
 * Procedural meshes (most of the game's built-at-runtime look) have no PSO precaching: the first frame one draws a
 * material it compiles that material's shaders on the spot, in play. So once the world's preload is in, every surface
 * material it brought is drawn once on a small box just ahead of the camera while the loading cover is black, for a few
 * rendered frames, then the boxes go. Clients only.
 */
UCLASS()
class WILDREACH_API UWRPerfWarmup : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/** Drawn and gone (or nothing to draw / skipped). */
	bool IsDone() const { return State == EState::Done; }

private:
	enum class EState : uint8 { Waiting, Drawing, Done };
	void Begin();
	void Place();
	void Finish(const TCHAR* Why);

	EState State = EState::Waiting;
	UPROPERTY(Transient) TObjectPtr<AActor> Rig;
	TArray<TWeakObjectPtr<UMaterialInterface>> Materials;
	uint64 StartFrame = 0;
	double StartedAt = 0.0;
};
