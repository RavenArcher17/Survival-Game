#include "WRPreload.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace WRPreloadDetail
{
	/** Every string under a JSON value that names something in the project's content ("/Game/..."). */
	void CollectGamePaths(const TSharedPtr<FJsonValue>& Value, TSet<FString>& Out)
	{
		if (!Value.IsValid())
		{
			return;
		}
		switch (Value->Type)
		{
		case EJson::String:
			if (Value->AsString().StartsWith(TEXT("/Game/")))
			{
				Out.Add(Value->AsString());
			}
			break;
		case EJson::Array:
			for (const TSharedPtr<FJsonValue>& V : Value->AsArray())
			{
				CollectGamePaths(V, Out);
			}
			break;
		case EJson::Object:
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
			{
				CollectGamePaths(Field.Value, Out);
			}
			break;
		default:
			break;
		}
	}
}

bool UWRPreloadSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TArray<FSoftObjectPath> UWRPreloadSubsystem::CollectPaths()
{
	TSet<FString> Strings;
	if (const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("preload.json")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Scan = nullptr;
		if (Root->TryGetArrayField(TEXT("scan"), Scan))
		{
			for (const TSharedPtr<FJsonValue>& File : *Scan)
			{
				if (const TSharedPtr<FJsonObject> Data = FWRTuning::LoadDataFile(File->AsString()))
				{
					WRPreloadDetail::CollectGamePaths(MakeShared<FJsonValueObject>(Data), Strings);
				}
			}
		}
		const TSharedPtr<FJsonValue>* Assets = Root->Values.Find(TEXT("assets"));
		if (Assets)
		{
			WRPreloadDetail::CollectGamePaths(*Assets, Strings);
		}
	}
	TArray<FSoftObjectPath> Paths;
	for (FString S : Strings)
	{
		// "/Game/A/b" names the package; the object inside has the same name.
		if (!S.Contains(TEXT(".")))
		{
			S = FString::Printf(TEXT("%s.%s"), *S, *FPaths::GetBaseFilename(S));
		}
		const FSoftObjectPath Path(S);
		if (Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			Paths.Add(Path);
		}
	}
	return Paths;
}

void UWRPreloadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (WRTune(TEXT("preload.enabled"), 1.f) <= 0.f || !UAssetManager::IsInitialized())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths = CollectPaths();
	if (Paths.Num() == 0)
	{
		return;
	}
	const int32 Count = Paths.Num();
	const double Started = FPlatformTime::Seconds();
	Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths),
		FStreamableDelegate::CreateWeakLambda(this, [Count, Started]()
		{
			UE_LOG(LogWildreach, Log, TEXT("PRELOAD %d assets loaded in the background in %.1f s"), Count, FPlatformTime::Seconds() - Started);
		}),
		FStreamableManager::DefaultAsyncLoadPriority, false, false, TEXT("WRPreload"));
	UE_LOG(LogWildreach, Log, TEXT("PRELOAD %d assets (Data/preload.json) requested"), Count);
}

void UWRPreloadSubsystem::Deinitialize()
{
	if (Handle.IsValid())
	{
		Handle->ReleaseHandle();
		Handle.Reset();
	}
	Super::Deinitialize();
}
