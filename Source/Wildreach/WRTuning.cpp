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
	StrOverrides.Reset();
	ApplyServerPreset();
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

void FWRTuning::ApplyServerPreset()
{
	// Preset: -wrpreset=<name>, else Saved/ServerDB/server.json {"preset": ..., "settings": {...}}, else the file's default.
	const TSharedPtr<FJsonObject> Presets = LoadDataFile(TEXT("server_presets.json"));
	const TSharedPtr<FJsonObject>* List = nullptr;
	if (!Presets.IsValid() || !Presets->TryGetObjectField(TEXT("presets"), List))
	{
		return;
	}
	TSharedPtr<FJsonObject> ServerFile;
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ServerDB"), TEXT("server.json"))))
	{
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), ServerFile);
	}
	Preset = Presets->GetStringField(TEXT("default"));
	if (ServerFile.IsValid()) ServerFile->TryGetStringField(TEXT("preset"), Preset);
	FParse::Value(FCommandLine::Get(), TEXT("wrpreset="), Preset);

	const auto ApplyObject = [this](const TSharedPtr<FJsonObject>& Settings)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Settings->Values)
		{
			const FString Key = TEXT("server.") + KV.Key;
			if (KV.Value->Type == EJson::String) StrOverrides.Add(Key, KV.Value->AsString());
			else if (KV.Value->Type == EJson::Boolean) Overrides.Add(Key, KV.Value->AsBool() ? 1.f : 0.f);
			else if (KV.Value->Type == EJson::Number) Overrides.Add(Key, KV.Value->AsNumber());
		}
	};
	// Custom starts from a base preset and takes the server file's settings; official presets are locked.
	FString Base = Preset;
	if (Preset == TEXT("custom") && ServerFile.IsValid()) ServerFile->TryGetStringField(TEXT("base"), Base);
	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (!(*List)->TryGetObjectField(Base == TEXT("custom") ? TEXT("pve") : Base, Values))
	{
		UE_LOG(LogWildreach, Warning, TEXT("Unknown server preset %s, using defaults"), *Base);
		return;
	}
	ApplyObject(*Values);
	const TSharedPtr<FJsonObject>* Custom = nullptr;
	if (Preset == TEXT("custom") && ServerFile.IsValid() && ServerFile->TryGetObjectField(TEXT("settings"), Custom))
	{
		ApplyObject(*Custom);
	}
	UE_LOG(LogWildreach, Log, TEXT("Server preset: %s"), *Preset);
}

FString FWRTuning::Str(const FString& Path, const FString& Fallback) const
{
	if (const FString* Override = StrOverrides.Find(Path))
	{
		return *Override;
	}
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
