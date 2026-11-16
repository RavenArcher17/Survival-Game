#include "WRPerf.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "WRPreload.h"
#include "WRMainMenu.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "PipelineStateCache.h"
#include "ProceduralMeshComponent.h"
#include "RHIStats.h"
#include "RenderTimer.h"
#include "RHIGlobals.h"
#include "Scalability.h"
#include "ShaderPipelineCache.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dxgi1_4.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "WRPerf"

namespace WRPerfDetail
{
	const TCHAR* Section = TEXT("Wildreach.Perf");

	float Tune(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("perf.")) + Key), Fallback); }

	/** The "perf" block of Data/tuning.json (null if missing). */
	TSharedPtr<FJsonObject> Block()
	{
		static TSharedPtr<FJsonObject> Cached;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			if (const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("tuning.json")))
			{
				const TSharedPtr<FJsonObject>* Perf = nullptr;
				if (Root->TryGetObjectField(TEXT("perf"), Perf)) Cached = *Perf;
			}
		}
		return Cached;
	}

	/** perf.<Object>.<Array>[Index], or the fallback. */
	float ArrayAt(const TCHAR* Object, const TCHAR* Array, int32 Index, float Fallback)
	{
		const TSharedPtr<FJsonObject> B = Block();
		const TSharedPtr<FJsonObject>* O = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (B && B->TryGetObjectField(Object, O) && (*O)->TryGetArrayField(Array, A) && A->Num() > 0)
		{
			return float((*A)[FMath::Clamp(Index, 0, A->Num() - 1)]->AsNumber());
		}
		return Fallback;
	}

	/** Machine-wide state (one game per process; read on the game thread). */
	bool bCovered = false;
	bool bCoverKnown = false;
	FText FirstRunNote;
	bool bOverlayLoaded = false;
	bool bOverlay = false;

	// ---- graphics memory
	double CachedCardGB = -2.0;      // -2: not asked yet
	WRPerf::FMemory CachedMemory;
	double NextMemoryAt = 0.0;

#if PLATFORM_WINDOWS
	/** The DXGI adapter the game renders on: the one whose name matches the RHI's, else the one with most memory. */
	IDXGIAdapter3* Adapter()
	{
		static TRefCountPtr<IDXGIAdapter3> Found;
		static bool bTried = false;
		if (bTried) return Found.GetReference();
		bTried = true;
		TRefCountPtr<IDXGIFactory1> Factory;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)Factory.GetInitReference())))
		{
			return nullptr;
		}
		TRefCountPtr<IDXGIAdapter1> Best;
		SIZE_T BestMemory = 0;
		TRefCountPtr<IDXGIAdapter1> Candidate;
		for (UINT i = 0; Factory->EnumAdapters1(i, Candidate.GetInitReference()) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 Desc;
			if (FAILED(Candidate->GetDesc1(&Desc)) || (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
			if (!GRHIAdapterName.IsEmpty() && GRHIAdapterName.Equals(FString(Desc.Description)))
			{
				Best = Candidate;
				break;
			}
			if (Desc.DedicatedVideoMemory > BestMemory)
			{
				BestMemory = Desc.DedicatedVideoMemory;
				Best = Candidate;
			}
		}
		if (Best.IsValid())
		{
			Best->QueryInterface(__uuidof(IDXGIAdapter3), (void**)Found.GetInitReference());
		}
		return Found.GetReference();
	}
#endif

	constexpr double GB = 1024.0 * 1024.0 * 1024.0;

	/** Shown names of the quality levels. */
	FText LevelName(int32 Level)
	{
		switch (Level)
		{
		case 0: return LOCTEXT("Low", "Low");
		case 1: return LOCTEXT("Medium", "Medium");
		case 2: return LOCTEXT("High", "High");
		case 3: return LOCTEXT("Epic", "Epic");
		case 4: return LOCTEXT("Cinematic", "Cinematic");
		default: return LOCTEXT("Custom", "Custom");
		}
	}

	/** The overall level, or the one every group shares, or -1 (mixed). */
	int32 OverallOf(const UGameUserSettings* S)
	{
		if (!S) return -1;
		if (S->GetOverallScalabilityLevel() >= 0) return S->GetOverallScalabilityLevel();
		const int32 Groups[] = { S->GetViewDistanceQuality(), S->GetShadowQuality(), S->GetGlobalIlluminationQuality(), S->GetReflectionQuality(),
			S->GetTextureQuality(), S->GetVisualEffectQuality(), S->GetFoliageQuality(), S->GetPostProcessingQuality(), S->GetAntiAliasingQuality() };
		for (const int32 G : Groups) if (G != Groups[0]) return -1;
		return Groups[0];
	}

	/** Not a session to touch: a server, a headless bot, a test tool. */
	bool IsPlayerSession()
	{
		if (GIsEditor || IsRunningDedicatedServer() || !FApp::CanEverRender() || GIsAutomationTesting) return false;
		const TCHAR* Cmd = FCommandLine::Get();
		return !FParse::Param(Cmd, TEXT("wrbot")) && !FParse::Param(Cmd, TEXT("unattended")) && !FParse::Param(Cmd, TEXT("wrnoautoperf"));
	}

	// ---- the frame time readout

	class SWRPerfOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRPerfOverlay) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			using namespace WRUI;
			SetVisibility(EVisibility::HitTestInvisible);
			const auto Part = [this](TFunction<FText()> Get, int32 Which)
			{
				return SNew(SBox).Padding(FMargin(0.f, 0.f, 10.f * U, 0.f))
				[
					WRUI::Text(TAttribute<FText>::CreateLambda(MoveTemp(Get)), WRUI::Body(12, 700),
						TAttribute<FSlateColor>::CreateLambda([this, Which]() { return FSlateColor(ColorOf(Which)); }), true)
				];
			};
			ChildSlot
			.HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(6.f * U, 2.f * U, 0.f, 0.f))
			[
				SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(8.f * U, 2.f * U, 0.f, 2.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return FText::Format(LOCTEXT("Fps", "{0} fps  {1} ms"), FText::AsNumber(FMath::RoundToInt(Fps)), Ms(Frame)); }, 0)]
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return FText::Format(LOCTEXT("Cpu", "CPU {0}"), Ms(Game)); }, 1)]
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return FText::Format(LOCTEXT("Draw", "Draw {0}"), Ms(Draw)); }, 2)]
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return FText::Format(LOCTEXT("Gpu", "GPU {0}"), Ms(Gpu)); }, 3)]
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return MemoryText(); }, 4)]
					+ SHorizontalBox::Slot().AutoWidth()[Part([this]() { return Shaders > 0 ? FText::Format(LOCTEXT("Shaders", "shaders {0}"), FText::AsNumber(Shaders)) : FText::GetEmpty(); }, 5)]
				]
			];
		}

		virtual void Tick(const FGeometry& Geometry, const double Time, const float Dt) override
		{
			SCompoundWidget::Tick(Geometry, Time, Dt);
			SumFrame += FApp::GetDeltaTime() * 1000.0;
			SumGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
			SumDraw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
			SumGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
			++Count;
			const double Now = FPlatformTime::Seconds();
			if (Now - WindowStart >= WRPerfDetail::Tune(TEXT("overlay.averageSeconds"), 0.5f) && Count > 0)
			{
				Frame = SumFrame / Count;
				Fps = Frame > 0.0 ? 1000.0 / Frame : 0.0;
				Game = SumGame / Count;
				Draw = SumDraw / Count;
				Gpu = SumGpu / Count;
				Shaders = WRPerf::ShadersPending();
				SumFrame = SumGame = SumDraw = SumGpu = 0.0;
				Count = 0;
				WindowStart = Now;
			}
		}

	private:
		static FText Ms(double V)
		{
			FNumberFormattingOptions O;
			O.MinimumFractionalDigits = 1;
			O.MaximumFractionalDigits = 1;
			return FText::AsNumber(V, &O);
		}

		FText MemoryText() const
		{
			const WRPerf::FMemory M = WRPerf::Memory();
			if (!M.IsKnown()) return FText::GetEmpty();
			FNumberFormattingOptions O;
			O.MinimumFractionalDigits = 1;
			O.MaximumFractionalDigits = 1;
			return FText::Format(LOCTEXT("Mem", "{0} / {1} GB"), FText::AsNumber(M.UsedGB, &O), FText::AsNumber(M.BudgetGB, &O));
		}

		/** The slowest of CPU / Draw / GPU in brass (the side holding the frame back); memory by its share. */
		FLinearColor ColorOf(int32 Which) const
		{
			if (Which == 4)
			{
				const WRPerf::FMemory M = WRPerf::Memory();
				const double Share = M.IsKnown() ? M.UsedGB / M.BudgetGB : 0.0;
				if (Share >= WRPerfDetail::Tune(TEXT("overlay.memoryDanger"), 0.95f)) return WRUI::Bad;
				if (Share >= WRPerfDetail::Tune(TEXT("overlay.memoryWarn"), 0.85f)) return WRUI::Warn;
				return WRUI::Ink2;
			}
			if (Which == 5) return WRUI::Warn;
			if (Which == 0) return WRUI::Ink;
			const double Worst = FMath::Max3(Game, Draw, Gpu);
			const double Mine = Which == 1 ? Game : Which == 2 ? Draw : Gpu;
			return Worst > 0.0 && Mine >= Worst ? WRUI::Brass : WRUI::Ink2;
		}

		double SumFrame = 0.0, SumGame = 0.0, SumDraw = 0.0, SumGpu = 0.0;
		int32 Count = 0;
		double WindowStart = 0.0;
		double Frame = 0.0, Fps = 0.0, Game = 0.0, Draw = 0.0, Gpu = 0.0;
		int32 Shaders = 0;
	};

	FAutoConsoleCommand OverlayCommand(TEXT("wr.perfoverlay"), TEXT("Show or hide the frame time readout (fps, CPU, draw and GPU ms, graphics memory). wr.perfoverlay [0|1]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			WRPerf::SetOverlay(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !WRPerf::OverlayOn());
		}));

	// ---- the warm-up's boxes

	/** One closed box (24 corners, so each face has its own normal) of half-size H around C, into a mesh section. */
	void AddBox(const FVector& C, float H, TArray<FVector>& V, TArray<int32>& T, TArray<FVector>& N, TArray<FVector2D>& UV, TArray<FProcMeshTangent>& Tan)
	{
		const FVector Axes[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
		for (int32 a = 0; a < 3; ++a)
		{
			for (const float Sign : { 1.f, -1.f })
			{
				const FVector Nrm = Axes[a] * Sign;
				const FVector U1 = Axes[(a + 1) % 3] * Sign;
				const FVector U2 = Axes[(a + 2) % 3];
				const int32 Base = V.Num();
				V.Add(C + (Nrm - U1 - U2) * H);
				V.Add(C + (Nrm + U1 - U2) * H);
				V.Add(C + (Nrm + U1 + U2) * H);
				V.Add(C + (Nrm - U1 + U2) * H);
				UV.Append({ FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) });
				for (int32 k = 0; k < 4; ++k)
				{
					N.Add(Nrm);
					Tan.Add(FProcMeshTangent(U1, false));
				}
				// Two triangles a face. A closed box shows faces to the camera whichever way they wind, so every pass draws it.
				T.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
			}
		}
	}
}

// ---------------------------------------------------------------- WRPerf

WRPerf::FMemory WRPerf::Memory()
{
	using namespace WRPerfDetail;
	const double Now = FPlatformTime::Seconds();
	if (Now < NextMemoryAt) return CachedMemory;
	NextMemoryAt = Now + 0.25;
	CachedMemory = FMemory();
	if (!FApp::CanEverRender()) return CachedMemory;
#if PLATFORM_WINDOWS
	if (IDXGIAdapter3* A = Adapter())
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO Info = {};
		if (SUCCEEDED(A->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &Info)) && Info.Budget > 0)
		{
			CachedMemory.UsedGB = double(Info.CurrentUsage) / GB;
			CachedMemory.BudgetGB = double(Info.Budget) / GB;
		}
	}
#endif
	return CachedMemory;
}

double WRPerf::CardGB()
{
	using namespace WRPerfDetail;
	if (CachedCardGB > -2.0) return CachedCardGB;
	CachedCardGB = -1.0;
	if (!FApp::CanEverRender()) return CachedCardGB;
#if PLATFORM_WINDOWS
	if (IDXGIAdapter3* A = Adapter())
	{
		DXGI_ADAPTER_DESC1 Desc;
		if (SUCCEEDED(A->GetDesc1(&Desc)) && Desc.DedicatedVideoMemory > 0)
		{
			CachedCardGB = double(Desc.DedicatedVideoMemory) / GB;
			return CachedCardGB;
		}
	}
#endif
	if (GDynamicRHI)
	{
		FTextureMemoryStats Stats;
		RHIGetTextureMemoryStats(Stats);
		if (Stats.DedicatedVideoMemory > 0) CachedCardGB = double(Stats.DedicatedVideoMemory) / GB;
	}
	return CachedCardGB;
}

FText WRPerf::AutoPick()
{
	using namespace WRPerfDetail;
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!S) return FText::GetEmpty();
	S->RunHardwareBenchmark();
	S->ApplyHardwareBenchmarkResults();
	// Then the card's memory caps what the benchmark (which measures speed, not memory) chose: the first tier the card
	// is below (perf.vramTiers, smallest first).
	const double Card = CardGB();
	FString Capped;
	const TSharedPtr<FJsonObject> B = Block();
	const TArray<TSharedPtr<FJsonValue>>* Tiers = nullptr;
	if (Card > 0.0 && B && B->TryGetArrayField(TEXT("vramTiers"), Tiers))
	{
		for (const TSharedPtr<FJsonValue>& V : *Tiers)
		{
			const TSharedPtr<FJsonObject> Tier = V->AsObject();
			if (!Tier || Card >= Tier->GetNumberField(TEXT("belowGB"))) continue;
			struct FGroup { const TCHAR* Key; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
			const FGroup Groups[] = {
				{ TEXT("textures"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
				{ TEXT("shadows"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
				{ TEXT("gi"), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
				{ TEXT("reflections"), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
				{ TEXT("effects"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
				{ TEXT("viewDistance"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
				{ TEXT("foliage"), &UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality },
				{ TEXT("post"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
			};
			for (const FGroup& G : Groups)
			{
				double Cap = 0.0;
				if (Tier->TryGetNumberField(G.Key, Cap) && (S->*G.Get)() > int32(Cap))
				{
					(S->*G.Set)(int32(Cap));
					Capped += FString::Printf(TEXT(" %s<=%d"), G.Key, int32(Cap));
				}
			}
			break;
		}
	}
	// The quality levels only: the window and resolution stay as they are (a -windowed start keeps its window).
	S->ApplyNonResolutionSettings();
	S->SaveSettings();
	const int32 Overall = OverallOf(S);
	UE_LOG(LogWildreach, Log, TEXT("PERF auto-detect: overall %d, card %.1f GB%s%s"), Overall, Card,
		Capped.IsEmpty() ? TEXT("") : TEXT(", capped:"), *Capped);
	return Overall >= 0
		? FText::Format(LOCTEXT("PickedLevel", "Graphics set to {0} for this computer"), LevelName(Overall))
		: LOCTEXT("PickedMixed", "Graphics set for this computer");
}

FText WRPerf::TakeFirstRunNote()
{
	FText Note = WRPerfDetail::FirstRunNote;
	WRPerfDetail::FirstRunNote = FText::GetEmpty();
	return Note;
}

int32 WRPerf::ShadersPending()
{
	if (!FApp::CanEverRender()) return 0;
	return int32(PipelineStateCache::NumActivePrecacheRequests()) + int32(FShaderPipelineCache::NumPrecompilesRemaining());
}

bool WRPerf::Ready(const UWorld* World)
{
	if (!World || !FApp::CanEverRender() || World->GetNetMode() == NM_DedicatedServer) return true;
	if (const UWRPreloadSubsystem* P = World->GetSubsystem<UWRPreloadSubsystem>(); P && !P->IsDone()) return false;
	if (const UWRPerfWarmup* W = World->GetSubsystem<UWRPerfWarmup>(); W && !W->IsDone()) return false;
	return ShadersPending() == 0;
}

FText WRPerf::Stage(const UWorld* World, bool bIslandIn)
{
	if (!bIslandIn) return LOCTEXT("StageIsland", "Loading the island…");
	if (World)
	{
		if (const UWRPreloadSubsystem* P = World->GetSubsystem<UWRPreloadSubsystem>(); P && !P->IsDone())
		{
			return LOCTEXT("StageArt", "Unpacking the island's creatures and gear…");
		}
	}
	const int32 Pending = ShadersPending();
	return Pending > 0
		? FText::Format(LOCTEXT("StageShadersN", "Preparing shaders for this computer… {0}"), FText::AsNumber(Pending))
		: LOCTEXT("StageShaders", "Preparing shaders for this computer…");
}

void WRPerf::NoteCovered(bool bCovered)
{
	using namespace WRPerfDetail;
	if (bCoverKnown && WRPerfDetail::bCovered == bCovered) return;
	bCoverKnown = true;
	WRPerfDetail::bCovered = bCovered;
	if (!FApp::CanEverRender()) return;
	// Under a black cover nothing is being played: compile as fast as the machine can (Lyra's loading screen does the
	// same). In play: the background batch size, so compiling never takes a frame.
	FShaderPipelineCache::SetBatchMode(bCovered ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);
	PipelineStateCache::PrecachePSOsBoostToHighestPriority(bCovered);
}

bool WRPerf::IsCovered()
{
	return WRPerfDetail::bCovered;
}

bool WRPerf::OverlayOn()
{
	using namespace WRPerfDetail;
	if (!bOverlayLoaded)
	{
		bOverlayLoaded = true;
		GConfig->GetBool(Section, TEXT("ShowFrameTime"), bOverlay, GGameUserSettingsIni);
	}
	return bOverlay;
}

void WRPerf::SetOverlay(bool bOn)
{
	using namespace WRPerfDetail;
	bOverlayLoaded = true;
	bOverlay = bOn;
	GConfig->SetBool(Section, TEXT("ShowFrameTime"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

// ---------------------------------------------------------------- UWRPerfSubsystem

bool UWRPerfSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UWRPerfSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FApp::CanEverRender()) return;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWRPerfSubsystem::Tick));
}

void UWRPerfSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

bool UWRPerfSubsystem::Tick(float DeltaTime)
{
	TickFirstRun();
	TickPool();
	TickOverlay();
	return true;
}

void UWRPerfSubsystem::TickPool()
{
	// The texture pool follows the texture level and the card (checked twice a second: the Video page changes the level).
	// Not in the editor's own process (Play in Editor): its texture pool is the editor's.
	if (GIsEditor) return;
	const double Now = FPlatformTime::Seconds();
	if (Now < NextPoolCheck) return;
	NextPoolCheck = Now + 0.5;
	const int32 Level = Scalability::GetQualityLevels().TextureQuality;
	const double Card = WRPerf::CardGB();
	if (Level == PoolLevel && Card == PoolCardGB) return;
	PoolLevel = Level;
	PoolCardGB = Card;
	if (Card <= 0.0) return;   // unknown card: the engine's own pool for the level
	const int32 L = FMath::Clamp(Level, 0, 3);
	const float Share = WRPerfDetail::ArrayAt(TEXT("texturePool"), TEXT("share"), L, 0.09f);
	const float Lo = WRPerfDetail::ArrayAt(TEXT("texturePool"), TEXT("min"), L, 400.f);
	const float Hi = WRPerfDetail::ArrayAt(TEXT("texturePool"), TEXT("max"), L, 1200.f);
	const int32 PoolMB = FMath::RoundToInt(FMath::Clamp(float(Card * 1024.0) * Share, Lo, FMath::Max(Lo, Hi)));
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
	{
		V->Set(PoolMB, ECVF_SetByCode);
		UE_LOG(LogWildreach, Log, TEXT("PERF texture pool %d MB (textures level %d, card %.1f GB)"), PoolMB, Level, Card);
	}
}

void UWRPerfSubsystem::TickFirstRun()
{
	// Once per session, the first time the main menu is up (test tools that go straight to a map never get here).
	if (bFirstRunDone || !SWRMainMenu::IsOpen()) return;
	bFirstRunDone = true;
	if (!WRPerfDetail::IsPlayerSession()) return;
	bool bPicked = false;
	GConfig->GetBool(WRPerfDetail::Section, TEXT("Picked"), bPicked, GGameUserSettingsIni);
	if (bPicked) return;
	GConfig->SetBool(WRPerfDetail::Section, TEXT("Picked"), true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	// Someone who already chose their levels (anything but the untouched Epic everywhere) keeps them.
	const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!S || WRPerfDetail::OverallOf(S) != 3)
	{
		UE_LOG(LogWildreach, Log, TEXT("PERF first launch: levels already chosen (%d), left as they are"), WRPerfDetail::OverallOf(S));
		return;
	}
	WRPerfDetail::FirstRunNote = WRPerf::AutoPick();
}

void UWRPerfSubsystem::TickOverlay()
{
	UGameViewportClient* VP = GEngine ? GEngine->GameViewport : nullptr;
	const bool bWant = VP && WRPerf::OverlayOn();
	if (bWant && Overlay.IsValid() && OverlayWorld.Get() != VP->GetWorld())
	{
		// A map change removed every viewport widget: put it back.
		VP->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		Overlay.Reset();
	}
	if (bWant && !Overlay.IsValid())
	{
		Overlay = SNew(WRPerfDetail::SWRPerfOverlay);
		VP->AddViewportWidgetContent(Overlay.ToSharedRef(), 70);
		OverlayWorld = VP->GetWorld();
	}
	else if (!bWant && Overlay.IsValid())
	{
		if (VP) VP->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		Overlay.Reset();
	}
}

// ---------------------------------------------------------------- UWRPerfWarmup

bool UWRPerfWarmup::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FApp::CanEverRender() && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UWRPerfWarmup::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRPerfWarmup, STATGROUP_Tickables);
}

void UWRPerfWarmup::Deinitialize()
{
	if (Rig) Rig->Destroy();
	Rig = nullptr;
	Super::Deinitialize();
}

void UWRPerfWarmup::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (State == EState::Done || !World || !World->HasBegunPlay()) return;
	if (State == EState::Waiting)
	{
		if (WRPerfDetail::Tune(TEXT("warmup.enabled"), 1.f) <= 0.f || World->GetNetMode() == NM_DedicatedServer)
		{
			Finish(TEXT("off"));
			return;
		}
		const UWRPreloadSubsystem* Preload = World->GetSubsystem<UWRPreloadSubsystem>();
		if (Preload && !Preload->IsDone()) return;
		const APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->PlayerCameraManager || !PC->IsLocalController()) return;
		if (!WRPerf::IsCovered())
		{
			// Boxes in front of the camera must never be seen: only under the black cover. A cover that has already
			// lifted (or never came, -oldui) gets a second to come back before the warm-up is given up.
			if (StartedAt <= 0.0) StartedAt = FPlatformTime::Seconds();
			if (FPlatformTime::Seconds() - StartedAt > 1.0) Finish(TEXT("skipped: the cover had lifted"));
			return;
		}
		Begin();
		return;
	}
	// Drawing: keep the boxes in view until they have been rendered for enough frames, or the cover starts to lift.
	if (!WRPerf::IsCovered())
	{
		Finish(TEXT("cover lifted"));
		return;
	}
	Place();
	if (GFrameCounter - StartFrame >= uint64(FMath::Max(1, FMath::RoundToInt(WRPerfDetail::Tune(TEXT("warmup.frames"), 6.f)))))
	{
		Finish(TEXT("done"));
	}
}

void UWRPerfWarmup::Begin()
{
	UWorld* World = GetWorld();
	TArray<UObject*> Loaded;
	if (const UWRPreloadSubsystem* Preload = World->GetSubsystem<UWRPreloadSubsystem>()) Preload->GetLoaded(Loaded);
	TSet<UMaterialInterface*> Seen;
	for (UObject* O : Loaded)
	{
		UMaterialInterface* M = Cast<UMaterialInterface>(O);
		const UMaterial* Base = M ? M->GetMaterial() : nullptr;
		if (!Base || Base->MaterialDomain != MD_Surface || Seen.Contains(M)) continue;
		Seen.Add(M);
		Materials.Add(M);
	}
	if (Materials.Num() == 0)
	{
		Finish(TEXT("no materials"));
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Rig = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Rig)
	{
		Finish(TEXT("no actor"));
		return;
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Rig, TEXT("WarmupBoxes"), RF_Transient);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Rig->SetRootComponent(Mesh);
	// Laid out to fit the view: a grid about 16:9 in shape, its cells as large as perf.warmup.size allows.
	const APlayerController* PC = World->GetFirstPlayerController();
	const float Fov = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f;
	const float Dist = WRPerfDetail::Tune(TEXT("warmup.distance"), 250.f);
	const float ViewW = 2.f * Dist * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Fov, 30.f, 120.f) * 0.5f));
	const float ViewH = ViewW * 9.f / 16.f;
	const int32 N = Materials.Num();
	const int32 Cols = FMath::Max(FMath::RoundToInt(WRPerfDetail::Tune(TEXT("warmup.columns"), 12.f)), FMath::CeilToInt(FMath::Sqrt(N * 16.f / 9.f)));
	const int32 Rows = FMath::DivideAndRoundUp(N, Cols);
	const float Cell = FMath::Min3(WRPerfDetail::Tune(TEXT("warmup.size"), 18.f) * 1.25f, 0.8f * ViewW / Cols, 0.8f * ViewH / FMath::Max(1, Rows));
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Col = i % Cols, Row = i / Cols;
		const FVector Centre(0.f, (Col - (Cols - 1) * 0.5f) * Cell, ((Rows - 1) * 0.5f - Row) * Cell);
		TArray<FVector> V, Nrm;
		TArray<int32> T;
		TArray<FVector2D> UV;
		TArray<FProcMeshTangent> Tan;
		WRPerfDetail::AddBox(Centre, Cell * 0.4f, V, T, Nrm, UV, Tan);
		Mesh->CreateMeshSection_LinearColor(i, V, T, Nrm, UV, TArray<FLinearColor>(), Tan, false);
		Mesh->SetMaterial(i, Materials[i].Get());
	}
	Mesh->RegisterComponent();
	State = EState::Drawing;
	StartFrame = GFrameCounter;
	StartedAt = FPlatformTime::Seconds();
	Place();
	UE_LOG(LogWildreach, Log, TEXT("PERF warm-up: drawing %d materials behind the cover"), N);
}

void UWRPerfWarmup::Place()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Rig || !PC || !PC->PlayerCameraManager) return;
	// Facing the camera, a little ahead of it, moved every frame (so the velocity pass draws them as well).
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator Look = PC->PlayerCameraManager->GetCameraRotation();
	Rig->SetActorLocationAndRotation(Eye + Look.Vector() * WRPerfDetail::Tune(TEXT("warmup.distance"), 250.f), Look);
}

void UWRPerfWarmup::Finish(const TCHAR* Why)
{
	if (State == EState::Drawing)
	{
		UE_LOG(LogWildreach, Log, TEXT("PERF warm-up: %d materials, %llu frames, %.2f s (%s)"), Materials.Num(), GFrameCounter - StartFrame, FPlatformTime::Seconds() - StartedAt, Why);
	}
	else
	{
		UE_LOG(LogWildreach, Log, TEXT("PERF warm-up: none (%s)"), Why);
	}
	if (Rig) Rig->Destroy();
	Rig = nullptr;
	Materials.Reset();
	State = EState::Done;
}

#undef LOCTEXT_NAMESPACE
