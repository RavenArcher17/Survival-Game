#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Read-only access to Data/tuning.json (and other Data/*.json files).
 * Every gameplay number goes through here so tuning never needs a code change.
 * Values are looked up by dotted path: WRTune(TEXT("weapon.lightTiming.windup"), 0.25f).
 * The fallback is used (and logged once) when the file or key is missing.
 */
class WILDREACH_API FWRTuning
{
public:
	static FWRTuning& Get();

	float Num(const FString& Path, float Fallback) const;
	FString Str(const FString& Path, const FString& Fallback) const;

	/** Re-read the file (console: WR.ReloadTuning). */
	void Reload();

	/** Loads any JSON file under the project's Data/ folder. Null on failure. */
	static TSharedPtr<FJsonObject> LoadDataFile(const FString& RelativePath);
	static FString DataDir();

private:
	FWRTuning();
	TSharedPtr<FJsonObject> Find(const FString& Path) const;
	TSharedPtr<FJsonObject> Root;
	mutable TSet<FString> WarnedKeys;
};

inline float WRTune(const TCHAR* Path, float Fallback) { return FWRTuning::Get().Num(Path, Fallback); }
