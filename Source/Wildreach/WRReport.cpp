// Report a problem (F8): the ring of recent events, the frozen frame, the context, sending, saving and forwarding to the
// host. WRReport.h has the overview; the panel is WRUIReport.cpp.

#include "WRReport.h"
#include "WRUIFit.h"
#include "WRGod.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPadInput.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPlayerUI.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

// The engine's running frame rate (not in a public header).
extern ENGINE_API float GAverageFPS;

// The panel (WRUIReport.cpp).
namespace WRReportUI
{
	TSharedRef<SWidget> Make(TSharedPtr<SWidget>& OutFocus);
}

namespace WRReportDetail
{
	using WRReport::FDraft;

	// ------------------------------------------------------------ the ring: the last 60 s
	struct FLine { double T = 0.0; FString K; FString M; };
	FCriticalSection RingLock;
	TArray<FLine> Ring;

	void Push(const FString& Kind, const FString& Msg)
	{
		const double T = FPlatformTime::Seconds();
		const double Keep = 60.0;
		FScopeLock Lock(&RingLock);
		Ring.Add({ T, Kind, Msg.Len() > 400 ? Msg.Left(400) + TEXT("...") : Msg });
		int32 Drop = 0;
		while (Drop < Ring.Num() && (Ring.Num() - Drop > 600 || T - Ring[Drop].T > Keep)) ++Drop;
		if (Drop > 0) Ring.RemoveAt(0, Drop, EAllowShrinking::No);
	}

	/** The game's own log lines (LogWildreach: notices, screens, combat, locks...) and anyone's warnings and errors. */
	class FRingDevice : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			static const FName Game(TEXT("LogWildreach"));
			const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
			if (Level <= ELogVerbosity::Warning) Push(Level <= ELogVerbosity::Error ? TEXT("error") : TEXT("warn"), FString::Printf(TEXT("%s: %s"), *Category.ToString(), V));
			else if (Category == Game && Level <= ELogVerbosity::Log) Push(TEXT("log"), V);
		}
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	};
	/** Kept for the whole run (GLog holds it; it's taken off GLog before exit). */
	FRingDevice* Device = nullptr;

	// ------------------------------------------------------------ the open report
	TSharedPtr<FDraft> Draft;
	TSharedPtr<SWidget> Panel;
	bool bCapturing = false;
	double CaptureDeadline = 0.0;
	FString CaptureVia;
	FDelegateHandle CaptureHandle;
	TArray<FColor> Captured;
	FIntPoint CapturedSize = FIntPoint::ZeroValue;
	bool bCaptured = false;
	int32 ShotCounter = 0;

	// ------------------------------------------------------------ the outbox and the trip to the host
	double NextOutbox = 5.0;   // (seconds after start: the first try waits for the network to settle)
	bool bPosting = false;
	struct FPart { int32 Id = 0; uint8 Kind = 0; int32 Part = 0; int32 Parts = 0; TArray<uint8> Bytes; };
	TArray<FPart> ToHost;
	struct FAssembly { TArray<uint8> Json, Jpeg; double Started = 0.0; };
	TMap<FString, FAssembly> FromClients;
	TMap<FString, double> LastFromClient;

	APlayerController* LocalPC()
	{
		UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	FString ReportsDir() { return FPaths::ProjectSavedDir() / TEXT("Reports"); }
	FString OutboxDir() { return ReportsDir() / TEXT("Outbox"); }

	FString Safe(const FString& In)
	{
		FString Out;
		for (const TCHAR C : In) Out.AppendChar(FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_') ? C : TEXT('_'));
		return Out.Left(32);
	}

	FString ToJson(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, W);
		return Out;
	}

	FString ToPrettyJson(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, W);
		return Out;
	}

	const TCHAR* NetModeName(ENetMode M)
	{
		return M == NM_Standalone ? TEXT("standalone") : M == NM_ListenServer ? TEXT("host") : M == NM_DedicatedServer ? TEXT("dedicated") : TEXT("client");
	}

	/** Everything a developer needs to find the problem (and the last 60 s). */
	TSharedRef<FJsonObject> BuildContext(APlayerController* PC)
	{
		TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		const FString Map = World ? World->GetMapName() : FString();
		C->SetStringField(TEXT("channel"), Map.Contains(TEXT("CombatArena")) ? TEXT("lab") : TEXT("main"));
		C->SetStringField(TEXT("build"), SWRMainMenu::GameVersion());
		C->SetStringField(TEXT("config"), LexToString(FApp::GetBuildConfiguration()));
		// The packaged build's own note (Tools\package.ps1 writes build.json beside the game), when there is one.
		for (const FString& Dir : { FPaths::ProjectDir(), FPaths::LaunchDir(), FPaths::RootDir() })
		{
			FString Text;
			TSharedPtr<FJsonObject> Info;
			if (FFileHelper::LoadFileToString(Text, *(Dir / TEXT("build.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Info) && Info.IsValid())
			{
				C->SetObjectField(TEXT("buildInfo"), Info);
				break;
			}
		}
		C->SetStringField(TEXT("map"), Map);
		C->SetStringField(TEXT("net"), World ? NetModeName(World->GetNetMode()) : TEXT("none"));
		C->SetStringField(TEXT("world"), WRWorlds::Current());
		C->SetStringField(TEXT("preset"), FWRTuning::Get().GetPreset());
		if (World && World->GetGameState()) C->SetNumberField(TEXT("players"), World->GetGameState()->PlayerArray.Num());
		C->SetBoolField(TEXT("mainMenu"), SWRMainMenu::IsOpen());

		if (PC)
		{
			TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			if (PC->PlayerState) P->SetStringField(TEXT("name"), PC->PlayerState->GetPlayerName());
			const FRotator View = PC->GetControlRotation();
			P->SetNumberField(TEXT("yaw"), FMath::RoundToInt(View.Yaw));
			P->SetNumberField(TEXT("pitch"), FMath::RoundToInt(View.Pitch));
			if (const APawn* Pawn = PC->GetPawn())
			{
				const FVector L = Pawn->GetActorLocation();
				P->SetStringField(TEXT("pos"), FString::Printf(TEXT("%.0f %.0f %.0f"), L.X, L.Y, L.Z));
				P->SetStringField(TEXT("pawn"), Pawn->GetClass()->GetName());
				if (const AWRCharacterBase* Body = Cast<AWRCharacterBase>(Pawn))
				{
					P->SetBoolField(TEXT("alive"), Body->IsAlive());
					if (const UWRAttributeSet* A = Body->GetAttributes())
					{
						P->SetStringField(TEXT("health"), FString::Printf(TEXT("%.0f/%.0f"), A->GetHealth(), A->GetMaxHealth()));
						P->SetStringField(TEXT("stamina"), FString::Printf(TEXT("%.0f/%.0f"), A->GetStamina(), A->GetMaxStamina()));
						P->SetNumberField(TEXT("hunger"), FMath::RoundToInt(A->GetHunger()));
						P->SetNumberField(TEXT("thirst"), FMath::RoundToInt(A->GetThirst()));
					}
					if (const UWRPathComponent* Paths = UWRPathComponent::FindFor(Body))
					{
						P->SetStringField(TEXT("path"), Paths->GetMainPath().ToString());
						P->SetNumberField(TEXT("level"), Paths->GetCharacterLevel());
					}
				}
			}
			if (const AWRPlayerController* W = Cast<AWRPlayerController>(PC))
			{
				P->SetNumberField(TEXT("menu"), static_cast<int32>(W->GetMenu()));
				if (const FWRPlayerUI* UI = W->GetUI()) P->SetStringField(TEXT("screen"), UI->GetScreen().ToString());
				P->SetBoolField(TEXT("placing"), W->IsPlacing());
				if (const AWRCharacterBase* Lock = W->GetLockTarget()) P->SetStringField(TEXT("lockOn"), Lock->GetName());
			}
			C->SetObjectField(TEXT("player"), P);
		}
		// A tester's session (the god mode, WRGod): who, what's on, and whether the character / world is marked tested,
		// so a report from it is never read as normal play.
		if (const TSharedPtr<FJsonObject> God = WRGod::ReportContext(PC)) C->SetObjectField(TEXT("godMode"), God);

		TSharedRef<FJsonObject> In = MakeShared<FJsonObject>();
		const FWRKeyBindings& K = FWRKeyBindings::Get();
		In->SetStringField(TEXT("device"), WRInput::UsingPad() ? TEXT("controller") : TEXT("keyboard and mouse"));
		In->SetBoolField(TEXT("padAttached"), FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached());
		In->SetNumberField(TEXT("mouseSensitivity"), K.MouseSensitivity());
		In->SetNumberField(TEXT("padSensitivity"), K.PadSensitivity());
		In->SetBoolField(TEXT("invertMouse"), K.InvertMouse());
		In->SetBoolField(TEXT("invertPad"), K.InvertPad());
		C->SetObjectField(TEXT("input"), In);

		TSharedRef<FJsonObject> Perf = MakeShared<FJsonObject>();
		Perf->SetNumberField(TEXT("fps"), FMath::RoundToInt(GAverageFPS));
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
			Perf->SetStringField(TEXT("viewport"), FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
		}
		if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			Perf->SetNumberField(TEXT("quality"), S->GetOverallScalabilityLevel());
			Perf->SetNumberField(TEXT("windowMode"), static_cast<int32>(S->GetFullscreenMode()));
			Perf->SetNumberField(TEXT("frameCap"), S->GetFrameRateLimit());
		}
		C->SetObjectField(TEXT("perf"), Perf);

		TSharedRef<FJsonObject> Dev = MakeShared<FJsonObject>();
		Dev->SetStringField(TEXT("os"), FPlatformMisc::GetOSVersion());
		Dev->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
		Dev->SetStringField(TEXT("gpu"), GRHIAdapterName);
		Dev->SetNumberField(TEXT("ramGB"), static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB));
		C->SetObjectField(TEXT("device"), Dev);

		C->SetNumberField(TEXT("playedSec"), FMath::RoundToInt(FPlatformTime::Seconds() - GStartTime));
		C->SetStringField(TEXT("at"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> Log;
		{
			const double Now = FPlatformTime::Seconds();
			FScopeLock Lock(&RingLock);
			for (const FLine& L : Ring)
			{
				TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
				E->SetNumberField(TEXT("s"), FMath::RoundToFloat(static_cast<float>(L.T - Now) * 10.f) / 10.f);
				E->SetStringField(TEXT("k"), L.K);
				E->SetStringField(TEXT("m"), L.M);
				Log.Add(MakeShared<FJsonValueObject>(E));
			}
		}
		C->SetArrayField(TEXT("log"), Log);
		return C;
	}

	/** The context as the inbox takes it from a player who isn't signed in (the game never is): at most ~4000
	 *  characters, so the oldest log lines go first, cut short, until it fits. The full one is in the saved file. */
	TSharedRef<FJsonObject> Compact(const TSharedRef<FJsonObject>& Full, int32 MaxChars)
	{
		TSharedRef<FJsonObject> C = MakeShared<FJsonObject>(*Full);
		TArray<TSharedPtr<FJsonValue>> Log = Full->GetArrayField(TEXT("log"));
		// Shorter lines first, then fewer.
		for (TSharedPtr<FJsonValue>& V : Log)
		{
			TSharedRef<FJsonObject> E = MakeShared<FJsonObject>(*V->AsObject());
			const FString M = E->GetStringField(TEXT("m"));
			if (M.Len() > 120) E->SetStringField(TEXT("m"), M.Left(120));
			V = MakeShared<FJsonValueObject>(E);
		}
		C->RemoveField(TEXT("buildInfo"));
		C->SetArrayField(TEXT("log"), Log);
		while (ToJson(C).Len() > MaxChars && Log.Num() > 0)
		{
			Log.RemoveAt(0, FMath::Max(1, Log.Num() / 8));
			C->SetArrayField(TEXT("log"), Log);
		}
		if (ToJson(C).Len() > MaxChars) C->RemoveField(TEXT("device"));
		return C;
	}

	FString LabelFor(int32 Kind)
	{
		// The inbox's labels (bug | balance | idea | praise | other): the nearest, so its filters work (as Waveraider's).
		return Kind == 6 ? TEXT("balance") : Kind == 7 ? TEXT("idea") : TEXT("bug");
	}

	void Notify(const FString& Text)
	{
		if (AWRPlayerController* W = Cast<AWRPlayerController>(LocalPC())) W->ShowLocalNotice(Text);
		UE_LOG(LogWildreach, Log, TEXT("REPORT %s"), *Text);
	}

	// ------------------------------------------------------------ posting
	void PostNext();

	void Post(const FString& File, const FString& Body)
	{
		bPosting = true;
		const FString Url = FWRTuning::Get().Str(TEXT("report.endpoint"), TEXT("https://freedom35studios.pages.dev/api/feedback"));
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetURL(Url);
		R->SetVerb(TEXT("POST"));
		R->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		R->SetHeader(TEXT("User-Agent"), TEXT("Wildreach/") + SWRMainMenu::GameVersion());
		R->SetTimeout(30.f);
		R->SetContentAsString(Body);
		R->OnProcessRequestComplete().BindLambda([File, Body](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
		{
			bPosting = false;
			const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (bOk && Code >= 200 && Code < 300)
			{
				IFileManager::Get().Delete(*File, false, true, true);
				UE_LOG(LogWildreach, Log, TEXT("REPORT sent to the studio (%d)"), Code);
				Notify(TEXT("Report sent: thank you"));
				NextOutbox = FPlatformTime::Seconds() + 1.0;   // the next waiting one, if any
				return;
			}
			// Rate limited, a server error, no endpoint or no network: keep it for later. A picture too big: once more without it.
			if (Code == 413)
			{
				TSharedPtr<FJsonObject> Obj;
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Obj) && Obj.IsValid() && Obj->HasField(TEXT("screenshot")))
				{
					Obj->RemoveField(TEXT("screenshot"));
					const FString Smaller = ToJson(Obj.ToSharedRef());
					FFileHelper::SaveStringToFile(Smaller, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
					NextOutbox = FPlatformTime::Seconds() + 1.0;
					return;
				}
			}
			const bool bRetry = Code == 0 || Code == 429 || Code >= 500 || Code == 404 || Code == 405;
			if (bRetry)
			{
				UE_LOG(LogWildreach, Log, TEXT("REPORT couldn't be sent (%d): it waits in %s and goes later"), Code, *File);
				NextOutbox = FPlatformTime::Seconds() + WRTune(TEXT("report.retrySeconds"), 30.f);
				return;
			}
			UE_LOG(LogWildreach, Warning, TEXT("REPORT refused by the studio (%d: %s); the copy in Saved/Reports stays"), Code, Response.IsValid() ? *Response->GetContentAsString().Left(200) : TEXT(""));
			IFileManager::Get().Delete(*File, false, true, true);
			NextOutbox = FPlatformTime::Seconds() + 1.0;
		});
		R->ProcessRequest();
	}

	void PostNext()
	{
		if (bPosting) return;
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(OutboxDir() / TEXT("*.json")), true, false);
		if (Files.Num() == 0)
		{
			NextOutbox = FPlatformTime::Seconds() + WRTune(TEXT("report.retrySeconds"), 30.f);
			return;
		}
		Files.Sort();
		const FString File = OutboxDir() / Files[0];
		FString Body;
		if (!FFileHelper::LoadFileToString(Body, *File))
		{
			IFileManager::Get().Delete(*File, false, true, true);
			return;
		}
		Post(File, Body);
	}

	void QueueToHost(int32 Id, uint8 Kind, const TArray<uint8>& Bytes)
	{
		const int32 Chunk = FMath::Clamp(static_cast<int32>(WRTune(TEXT("report.forwardChunkBytes"), 8192.f)), 1024, 32768);
		const int32 Parts = FMath::Max(1, FMath::DivideAndRoundUp(Bytes.Num(), Chunk));
		for (int32 i = 0; i < Parts; ++i)
		{
			FPart& P = ToHost.AddDefaulted_GetRef();
			P.Id = Id;
			P.Kind = Kind;
			P.Part = i;
			P.Parts = Parts;
			const int32 From = i * Chunk;
			P.Bytes.Append(Bytes.GetData() + From, FMath::Min(Chunk, Bytes.Num() - From));
		}
	}

	// ------------------------------------------------------------ the frozen frame
	void BuildShot(FDraft& D)
	{
		if (!bCaptured || CapturedSize.X <= 0 || CapturedSize.Y <= 0 || Captured.Num() != CapturedSize.X * CapturedSize.Y) return;
		const int32 MaxW = FMath::Max(320, static_cast<int32>(WRTune(TEXT("report.shotMaxWidth"), 1280.f)));
		int32 W = CapturedSize.X, H = CapturedSize.Y;
		TArray<FColor> Pixels;
		if (W > MaxW)
		{
			const int32 NW = MaxW, NH = FMath::Max(1, FMath::RoundToInt(static_cast<float>(H) * MaxW / W));
			FImageUtils::ImageResize(W, H, Captured, NW, NH, Pixels, false);
			W = NW;
			H = NH;
		}
		else
		{
			Pixels = MoveTemp(Captured);
		}
		for (FColor& P : Pixels) P.A = 255;
		IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Jpg = Wrappers.CreateImageWrapper(EImageFormat::JPEG);
		const int32 MaxBytes = static_cast<int32>(WRTune(TEXT("report.shotMaxBytes"), 330000.f));
		for (int32 Q = 75; Jpg.IsValid() && Q >= 35; Q -= 10)
		{
			if (!Jpg->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), W, H, ERGBFormat::BGRA, 8)) break;
			const TArray64<uint8> Bytes = Jpg->GetCompressed(Q);
			D.Jpeg = TArray<uint8>(Bytes.GetData(), static_cast<int32>(Bytes.Num()));
			if (D.Jpeg.Num() <= MaxBytes) break;
		}
		if (D.Jpeg.Num() > 0)
		{
			D.Shot = FBase64::Encode(D.Jpeg);
			D.Width = W;
			D.Height = H;
			TArray<uint8> Raw;
			Raw.Append(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor));
			D.Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(*FString::Printf(TEXT("WRReportShot_%d"), ++ShotCounter)), FVector2D(W, H), Raw);
		}
		Captured.Reset();
		bCaptured = false;
	}

	void OpenPanel()
	{
		bCapturing = false;
		if (CaptureHandle.IsValid())
		{
			UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);
			CaptureHandle.Reset();
		}
		APlayerController* PC = LocalPC();
		if (!PC || !GEngine || !GEngine->GameViewport) return;
		Draft = MakeShared<FDraft>();
		Draft->Via = CaptureVia;
		BuildShot(*Draft);
		Draft->Context = BuildContext(PC);
		Draft->Context->SetStringField(TEXT("via"), CaptureVia);
		// Alone, the world waits while you write (a client or host can't stop the others' game).
		UWorld* World = PC->GetWorld();
		if (World && World->GetNetMode() == NM_Standalone && !SWRMainMenu::IsOpen() && !UGameplayStatics::IsGamePaused(World))
		{
			Draft->bPaused = UGameplayStatics::SetGamePaused(World, true);
		}
		TSharedPtr<SWidget> Focus;
		Panel = WRUI::Fit(WRReportUI::Make(Focus));
		GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(), 1000);
		WRInput::AddNavRoot(Panel.ToSharedRef(), 1000);
		WRInput::BeginOverlay(Focus);
		UE_LOG(LogWildreach, Log, TEXT("REPORT open (%s; picture %dx%d, %d bytes)"), *CaptureVia, Draft->Width, Draft->Height, Draft->Jpeg.Num());
	}

	void ClosePanel()
	{
		if (Panel.IsValid())
		{
			WRInput::RemoveNavRoot(Panel);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
			Panel.Reset();
			WRInput::EndOverlay();
		}
		if (Draft.IsValid() && Draft->bPaused)
		{
			if (APlayerController* PC = LocalPC()) UGameplayStatics::SetGamePaused(PC, false);
		}
		Draft.Reset();
	}
}

namespace WRReportCrash
{
	FString CrashDir;   // the last run's crash folder, until answered
	bool bChecked = false;
	bool bSwitchedShadows = false;   // this start turned classic shadows on after a graphics-card crash
	const TCHAR* GraphicsSection = TEXT("Wildreach.Graphics");

	/** r.Shadow.Virtual.Enable from the setting (by code: the project's renderer settings turn it on). */
	void ApplyShadowMethod(bool bClassic)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable")))
		{
			V->Set(bClassic ? 0 : 1, ECVF_SetByCode);
			UE_LOG(LogWildreach, Log, TEXT("REPORT shadows: %s"), bClassic ? TEXT("classic shadow maps") : TEXT("virtual shadow maps"));
		}
	}
	FString StampDir() { return FPaths::ProjectSavedDir() / TEXT("Wildreach"); }
	FString SessionFile() { return StampDir() / TEXT("session"); }
	FString CleanFile() { return StampDir() / TEXT("clean_exit"); }
	const TCHAR* Section = TEXT("Wildreach.Report");

	/** The text between <Tag> and </Tag> in the crash context (empty when it isn't there). */
	FString Between(const FString& Xml, const TCHAR* Tag)
	{
		const FString Open = FString::Printf(TEXT("<%s>"), Tag), Close = FString::Printf(TEXT("</%s>"), Tag);
		const int32 A = Xml.Find(Open);
		if (A == INDEX_NONE) return FString();
		const int32 B = Xml.Find(Close, ESearchCase::CaseSensitive, ESearchDir::FromStart, A);
		return B == INDEX_NONE ? FString() : Xml.Mid(A + Open.Len(), B - A - Open.Len()).TrimStartAndEnd();
	}

	/** Once at start, in the game only (the editor's play sessions aren't runs of the game). */
	void CheckLastRun()
	{
		// Once a run, whichever asks first (the log's install or the main menu's crash card).
		if (bChecked || GIsEditor) return;
		bChecked = true;
		IFileManager& FM = IFileManager::Get();
		FM.MakeDirectory(*StampDir(), true);
		const FDateTime LastStart = FM.GetTimeStamp(*SessionFile());
		const FDateTime CleanAt = FM.GetTimeStamp(*CleanFile());
		const bool bHadRun = LastStart != FDateTime::MinValue();
		const bool bClean = CleanAt != FDateTime::MinValue() && CleanAt >= LastStart;
		if (bHadRun && !bClean)
		{
			// The newest crash folder made after that run began and not answered before.
			FString Seen;
			GConfig->GetString(Section, TEXT("LastCrashSeen"), Seen, GGameUserSettingsIni);
			const FString CrashRoot = FPaths::ProjectSavedDir() / TEXT("Crashes");
			TArray<FString> Dirs;
			FM.FindFiles(Dirs, *(CrashRoot / TEXT("*")), false, true);
			FDateTime Best = LastStart;
			for (const FString& D : Dirs)
			{
				const FDateTime T = FM.GetTimeStamp(*(CrashRoot / D));
				// A handled ensure writes a folder too (the engine's PSO and mesh checks do on most starts): not a crash.
				FString Context;
				FFileHelper::LoadFileToString(Context, *(CrashRoot / D / TEXT("CrashContext.runtime-xml")));
				if (Context.Contains(TEXT("<CrashType>Ensure</CrashType>"))) continue;
				if (T > Best && D != Seen)
				{
					Best = T;
					CrashDir = CrashRoot / D;
				}
			}
			if (!CrashDir.IsEmpty())
			{
				FString Context;
				FFileHelper::LoadFileToString(Context, *(CrashDir / TEXT("CrashContext.runtime-xml")));
				const bool bGpu = Between(Context, TEXT("CrashType")) == TEXT("GPUCrash");
				UE_LOG(LogWildreach, Log, TEXT("REPORT the last run crashed%s: %s"), bGpu ? TEXT(" (the graphics card)") : TEXT(""), *CrashDir);
				// The graphics card stopped: classic shadow maps from now on (see WRReport::ClassicShadows), once.
				if (bGpu && !WRReport::ClassicShadows() && !FParse::Param(FCommandLine::Get(), TEXT("wrbot")))
				{
					GConfig->SetBool(GraphicsSection, TEXT("ClassicShadows"), true, GGameUserSettingsIni);
					GConfig->Flush(false, GGameUserSettingsIni);
					bSwitchedShadows = true;
					UE_LOG(LogWildreach, Log, TEXT("REPORT the graphics card crashed last run: classic shadows from now on (Settings > Video)"));
				}
			}
		}
		// (Bot runs, -wrbot: the tests and picture tools draw what the project draws; this machine's setting is the player's.)
		if (WRReport::ClassicShadows() && !FParse::Param(FCommandLine::Get(), TEXT("wrbot"))) ApplyShadowMethod(true);
		// This run starts now; a normal quit stamps clean_exit.
		FM.Delete(*CleanFile(), false, true, true);
		FFileHelper::SaveStringToFile(FDateTime::UtcNow().ToIso8601(), *SessionFile());
		FCoreDelegates::OnPreExit.AddLambda([]()
		{
			if (!GIsCriticalError) FFileHelper::SaveStringToFile(FDateTime::UtcNow().ToIso8601(), *CleanFile());
		});
	}

	void Answer()
	{
		if (CrashDir.IsEmpty()) return;
		GConfig->SetString(Section, TEXT("LastCrashSeen"), *FPaths::GetCleanFilename(CrashDir), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		CrashDir.Empty();
	}
}

void WRReport::InstallLog()
{
	using namespace WRReportDetail;
	if (Device || !GLog) return;
	Device = new FRingDevice();
	GLog->AddOutputDevice(Device);
	FCoreDelegates::OnPreExit.AddLambda([]() { if (GLog && Device) GLog->RemoveOutputDevice(Device); });
	WRReportCrash::CheckLastRun();
}

FString WRReport::PendingCrash()
{
	WRReportCrash::CheckLastRun();
	return WRReportCrash::CrashDir;
}

bool WRReport::ClassicShadows()
{
	bool bOn = false;
	if (GConfig) GConfig->GetBool(WRReportCrash::GraphicsSection, TEXT("ClassicShadows"), bOn, GGameUserSettingsIni);
	return bOn;
}

void WRReport::SetClassicShadows(bool bOn)
{
	if (!GConfig) return;
	GConfig->SetBool(WRReportCrash::GraphicsSection, TEXT("ClassicShadows"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	WRReportCrash::ApplyShadowMethod(bOn);
}

bool WRReport::SwitchedShadowsAfterCrash()
{
	WRReportCrash::CheckLastRun();
	return WRReportCrash::bSwitchedShadows;
}

void WRReport::DismissCrash()
{
	UE_LOG(LogWildreach, Log, TEXT("REPORT crash card: not now"));
	WRReportCrash::Answer();
}

void WRReport::SendCrash()
{
	using namespace WRReportDetail;
	const FString Dir = WRReportCrash::CrashDir;
	if (Dir.IsEmpty()) return;
	IFileManager& FM = IFileManager::Get();
	// What crashed and where: the crash context's message and call stack, and the end of the log it kept.
	FString Xml;
	FFileHelper::LoadFileToString(Xml, *(Dir / TEXT("CrashContext.runtime-xml")));
	const FString Error = WRReportCrash::Between(Xml, TEXT("ErrorMessage")).Left(600);
	const FString Stack = WRReportCrash::Between(Xml, TEXT("CallStack")).Left(1200);
	TArray<FString> Logs;
	FM.FindFiles(Logs, *(Dir / TEXT("*.log")), true, false);
	FString Tail;
	if (Logs.Num() > 0)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *(Dir / Logs[0]));
		const int32 Keep = static_cast<int32>(WRTune(TEXT("report.crashLogLines"), 40.f));
		for (int32 i = FMath::Max(0, Lines.Num() - Keep); i < Lines.Num(); ++i) Tail += Lines[i].Left(200) + TEXT("\n");
		Tail = Tail.Right(1200);
	}
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	const FString Name = FString::Printf(TEXT("%s_crash_%s"), *Stamp, *Safe(FPaths::GetCleanFilename(Dir)));
	const FString Body = FString::Printf(TEXT("[Crash] Wildreach closed unexpectedly on the last run. %s"), Error.IsEmpty() ? TEXT("(no message)") : *Error.Left(300));
	TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();
	Context->SetStringField(TEXT("build"), SWRMainMenu::GameVersion());
	Context->SetStringField(TEXT("category"), TEXT("Crash"));
	Context->SetStringField(TEXT("crashFolder"), FPaths::GetCleanFilename(Dir));
	Context->SetStringField(TEXT("error"), Error);
	Context->SetStringField(TEXT("callstack"), Stack);
	Context->SetStringField(TEXT("logTail"), Tail);
	Context->SetStringField(TEXT("gpu"), GRHIAdapterName);
	Context->SetStringField(TEXT("os"), FPlatformMisc::GetOSVersion());
	// The full copy on this machine, then the inbox through the outbox (the context trimmed to what it takes).
	TSharedRef<FJsonObject> Full = MakeShared<FJsonObject>();
	Full->SetStringField(TEXT("game"), TEXT("wildreach"));
	Full->SetStringField(TEXT("category"), TEXT("Crash"));
	Full->SetStringField(TEXT("body"), Body);
	Full->SetObjectField(TEXT("context"), Context);
	FM.MakeDirectory(*ReportsDir(), true);
	FFileHelper::SaveStringToFile(ToPrettyJson(Full), *(ReportsDir() / (Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TSharedRef<FJsonObject> Post = MakeShared<FJsonObject>();
	Post->SetStringField(TEXT("game"), FWRTuning::Get().Str(TEXT("report.game"), TEXT("wildreach")));
	Post->SetStringField(TEXT("body"), Body);
	Post->SetStringField(TEXT("category"), TEXT("Bug"));
	Post->SetStringField(TEXT("label"), TEXT("bug"));
	Post->SetObjectField(TEXT("context"), Context);   // already inside the inbox's ~4000 characters (the parts are cut above)
	FM.MakeDirectory(*OutboxDir(), true);
	FFileHelper::SaveStringToFile(ToJson(Post), *(OutboxDir() / (Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	NextOutbox = 0.0;
	UE_LOG(LogWildreach, Log, TEXT("REPORT crash report queued: %s"), *Name);
	WRReportCrash::Answer();
}

void WRReport::Event(const TCHAR* Kind, const FString& Text)
{
	using namespace WRReportDetail;
	Push(Kind, Text);
}

const TArray<FString>& WRReport::Kinds()
{
	using namespace WRReportDetail;
	static const TArray<FString> K = { TEXT("Bug"), TEXT("Stuck"), TEXT("Controls"), TEXT("Camera"), TEXT("Music-Audio"), TEXT("Visual"), TEXT("Balance"), TEXT("Idea") };
	return K;
}

bool WRReport::IsOpen() { return WRReportDetail::Panel.IsValid() || WRReportDetail::bCapturing; }
TSharedPtr<WRReport::FDraft> WRReport::GetDraft() { return WRReportDetail::Draft; }

void WRReport::Open(const TCHAR* Via)
{
	using namespace WRReportDetail;
	if (IsOpen() || !GEngine || !GEngine->GameViewport || !LocalPC()) return;
	bCapturing = true;
	bCaptured = false;
	CaptureVia = Via;
	Captured.Reset();
	// The next frame drawn (this one, with the interface) comes back here instead of going to a file.
	CaptureHandle = UGameViewportClient::OnScreenshotCaptured().AddLambda([](int32 W, int32 H, const TArray<FColor>& Colors)
	{
		Captured = Colors;
		CapturedSize = FIntPoint(W, H);
		bCaptured = true;
	});
	FScreenshotRequest::RequestScreenshot(true);
	CaptureDeadline = FPlatformTime::Seconds() + 1.0;
	UE_LOG(LogWildreach, Log, TEXT("REPORT asked for (%s)"), Via);
}

void WRReport::Cancel()
{
	using namespace WRReportDetail;
	if (Panel.IsValid()) UE_LOG(LogWildreach, Log, TEXT("REPORT cancelled"));
	ClosePanel();
}

void WRReport::Send(int32 Kind, const FString& Note)
{
	using namespace WRReportDetail;
	if (!Draft.IsValid()) return;
	const TArray<FString>& All = Kinds();
	Kind = FMath::Clamp(Kind, 0, All.Num() - 1);
	const FString Category = All[Kind];
	const FString Clean = Note.TrimStartAndEnd().Left(3500);
	APlayerController* PC = LocalPC();
	const FString Player = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(TEXT("player"));
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	const FString Name = FString::Printf(TEXT("%s_%s_%s"), *Stamp, *Safe(Player), *Safe(Category));
	const FString Body = FString::Printf(TEXT("[%s] %s"), *Category, Clean.IsEmpty() ? TEXT("(no note)") : *Clean);
	TSharedRef<FJsonObject> Context = Draft->Context.ToSharedRef();
	Context->SetStringField(TEXT("category"), Category);
	Context->SetStringField(TEXT("savedAs"), Name);
	Context->SetBoolField(TEXT("screenshot"), !Draft->Shot.IsEmpty());

	// 1. The full report on this machine (Saved/Reports).
	TSharedRef<FJsonObject> Full = MakeShared<FJsonObject>();
	Full->SetStringField(TEXT("game"), TEXT("wildreach"));
	Full->SetStringField(TEXT("category"), Category);
	Full->SetStringField(TEXT("note"), Clean);
	Full->SetStringField(TEXT("body"), Body);
	Full->SetObjectField(TEXT("context"), Context);
	const FString FullJson = ToPrettyJson(Full);
	IFileManager::Get().MakeDirectory(*ReportsDir(), true);
	FFileHelper::SaveStringToFile(FullJson, *(ReportsDir() / (Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (Draft->Jpeg.Num() > 0) FFileHelper::SaveArrayToFile(Draft->Jpeg, *(ReportsDir() / (Name + TEXT(".jpg"))));
	UE_LOG(LogWildreach, Log, TEXT("REPORT saved: %s"), *(ReportsDir() / Name));

	// 2. The studio's inbox, through the outbox (sent now, or later if it can't be).
	TSharedRef<FJsonObject> Post = MakeShared<FJsonObject>();
	Post->SetStringField(TEXT("game"), FWRTuning::Get().Str(TEXT("report.game"), TEXT("wildreach")));
	Post->SetStringField(TEXT("body"), Body);
	Post->SetStringField(TEXT("category"), Category);
	Post->SetStringField(TEXT("label"), LabelFor(Kind));
	Post->SetObjectField(TEXT("context"), Compact(Context, static_cast<int32>(WRTune(TEXT("report.contextMaxChars"), 3900.f))));
	if (!Draft->Shot.IsEmpty()) Post->SetStringField(TEXT("screenshot"), TEXT("data:image/jpeg;base64,") + Draft->Shot);
	IFileManager::Get().MakeDirectory(*OutboxDir(), true);
	FFileHelper::SaveStringToFile(ToJson(Post), *(OutboxDir() / (Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	NextOutbox = 0.0;

	// 3. A client's report goes to the host too.
	if (PC && PC->GetNetMode() == NM_Client)
	{
		const int32 Id = FMath::Rand();
		if (Draft->Jpeg.Num() > 0) QueueToHost(Id, 1, Draft->Jpeg);
		FTCHARToUTF8 Utf8(*FullJson);
		TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		QueueToHost(Id, 0, Bytes);   // last: the host writes the report when this arrives
	}
	WRReport::Event(TEXT("report"), Body.Left(120));
	ClosePanel();
	Notify(TEXT("Report saved: sending it to the studio"));
}

void WRReport::Tick(float DeltaTime)
{
	using namespace WRReportDetail;
	const double Now = FPlatformTime::Seconds();
	if (bCapturing && (bCaptured || Now > CaptureDeadline)) OpenPanel();
	// A panel left open when the map changed (its world went away): close it cleanly.
	if (Panel.IsValid() && !LocalPC()) ClosePanel();
	if (!bPosting && Now >= NextOutbox) PostNext();

	if (ToHost.Num() > 0)
	{
		AWRPlayerController* W = Cast<AWRPlayerController>(LocalPC());
		if (!W || W->GetNetMode() != NM_Client)
		{
			ToHost.Reset();   // (left the game: this machine keeps its own copy)
			return;
		}
		int32 Budget = static_cast<int32>(WRTune(TEXT("report.forwardBytesPerTick"), 8192.f));
		while (ToHost.Num() > 0 && Budget > 0)
		{
			const FPart& P = ToHost[0];
			W->ServerReportPart(P.Id, P.Part, P.Parts, P.Kind, P.Bytes);
			Budget -= P.Bytes.Num();
			ToHost.RemoveAt(0);
		}
	}
}

void WRReport::HostReceive(APlayerController* From, int32 Id, int32 Part, int32 Parts, uint8 Kind, const TArray<uint8>& Bytes)
{
	using namespace WRReportDetail;
	if (!From || Kind > 1 || Parts <= 0 || Parts > 400 || Part < 0 || Part >= Parts) return;
	const FString Player = From->PlayerState ? From->PlayerState->GetPlayerName() : FString(TEXT("player"));
	const FString Key = FString::Printf(TEXT("%s#%d"), *Player, Id);
	const double Now = FPlatformTime::Seconds();
	// A player can't flood the host: one report at a time, a few a minute, each under 2 MB.
	for (auto It = FromClients.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().Started > 120.0) It.RemoveCurrent();
	}
	FAssembly* A = FromClients.Find(Key);
	if (!A)
	{
		if (const double* Last = LastFromClient.Find(Player); Last && Now - *Last < WRTune(TEXT("report.hostMinSeconds"), 10.f)) return;
		LastFromClient.Add(Player, Now);
		A = &FromClients.Add(Key);
		A->Started = Now;
	}
	TArray<uint8>& Into = Kind == 0 ? A->Json : A->Jpeg;
	if (Into.Num() + Bytes.Num() > 2 * 1024 * 1024)
	{
		FromClients.Remove(Key);
		return;
	}
	Into.Append(Bytes);
	if (Kind != 0 || Part != Parts - 1) return;

	// The JSON comes last: write it all.
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	const FString Name = FString::Printf(TEXT("%s_%s_from-client"), *Stamp, *Safe(Player));
	IFileManager::Get().MakeDirectory(*ReportsDir(), true);
	FFileHelper::SaveArrayToFile(A->Json, *(ReportsDir() / (Name + TEXT(".json"))));
	if (A->Jpeg.Num() > 0) FFileHelper::SaveArrayToFile(A->Jpeg, *(ReportsDir() / (Name + TEXT(".jpg"))));
	FromClients.Remove(Key);
	UE_LOG(LogWildreach, Log, TEXT("REPORT from %s saved: %s"), *Player, *(ReportsDir() / Name));
	// The host's own player hears of it (a listen server; a dedicated one has the log).
	if (UWorld* World = From->GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AWRPlayerController* Host = Cast<AWRPlayerController>(It->Get()); Host && Host->IsLocalController())
			{
				Host->ShowLocalNotice(FString::Printf(TEXT("%s sent a bug report (saved in Saved/Reports)"), *Player));
			}
		}
	}
}
