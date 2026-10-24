#include "WRTuning.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static FAutoConsoleCommand GReloadTuningCmd(
	TEXT("WR.ReloadTuning"),
	TEXT("Re-read Data/tuning.json"),
	FConsoleCommandDelegate::CreateLambda([]() { FWRTuning::Get().Reload(); }));

FWRTuning& FWRTuning::Get()
{
	static FWRTuning Instance;
	return Instance;
}

FWRTuning::FWRTuning()
{
	Reload();
}

FString FWRTuning::DataDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
}

TSharedPtr<FJsonObject> FWRTuning::LoadDataFile(const FString& RelativePath)
{
	FString Text;
	const FString Full = FPaths::Combine(DataDir(), RelativePath);
	if (!FFileHelper::LoadFileToString(Text, *Full))
	{
		UE_LOG(LogWildreach, Warning, TEXT("Data file missing: %s"), *Full);
		return nullptr;
	}

	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("Data file is not valid JSON: %s"), *Full);
		return nullptr;
	}
	return Obj;
}

void FWRTuning::Reload()
{
	Root = LoadDataFile(TEXT("tuning.json"));
	WarnedKeys.Reset();
	Overrides.Reset();
	FString Cli;
	if (FParse::Value(FCommandLine::Get(), TEXT("wrtune="), Cli, false))
	{
		TArray<FString> Pairs;
		Cli.ParseIntoArray(Pairs, TEXT(","));
		for (const FString& P : Pairs)
		{
			FString Key, Value;
			if (P.Split(TEXT(":"), &Key, &Value))
			{
				Overrides.Add(Key, FCString::Atof(*Value));
				UE_LOG(LogWildreach, Log, TEXT("Tuning override %s = %s"), *Key, *Value);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Tuning %s"), Root.IsValid() ? TEXT("loaded") : TEXT("NOT loaded, using code fallbacks"));
}

TSharedPtr<FJsonObject> FWRTuning::Find(const FString& Path) const
{
	// Returns the object that holds the last path segment.
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	TSharedPtr<FJsonObject> Cur = Root;
	for (int32 i = 0; Cur.IsValid() && i < Parts.Num() - 1; ++i)
	{
		const TSharedPtr<FJsonObject>* Next = nullptr;
		Cur = Cur->TryGetObjectField(Parts[i], Next) ? *Next : nullptr;
	}
	return Cur;
}

float FWRTuning::Num(const FString& Path, float Fallback) const
{
	if (const float* Override = Overrides.Find(Path))
	{
		return *Override;
	}
	if (const TSharedPtr<FJsonObject> Holder = Find(Path))
	{
		FString Leaf;
		Path.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Leaf.IsEmpty())
		{
			Leaf = Path;
		}
		double Value = 0.0;
		if (Holder->TryGetNumberField(Leaf, Value))
		{
			return static_cast<float>(Value);
		}
	}
	if (!WarnedKeys.Contains(Path))
	{
		WarnedKeys.Add(Path);
		UE_LOG(LogWildreach, Verbose, TEXT("Tuning key %s missing, using %.3f"), *Path, Fallback);
	}
	return Fallback;
}

FString FWRTuning::Str(const FString& Path, const FString& Fallback) const
{
	if (const TSharedPtr<FJsonObject> Holder = Find(Path))
	{
		FString Leaf;
		Path.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		FString Value;
		if (Holder->TryGetStringField(Leaf.IsEmpty() ? Path : Leaf, Value))
		{
			return Value;
		}
	}
	return Fallback;
}
