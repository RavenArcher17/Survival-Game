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

	/** Re-read the file (console: WR.ReloadTuning) and the -wrtune=key:value,key:value overrides. */
	void Reload();
	/** Override a number at runtime (server settings presets, tests). */
	void Set(const FString& Path, float Value) { Overrides.Add(Path, Value); }
	/** Active server preset (Data/server_presets.json): pvp_standard, pve, casual_coop, playtest or custom. */
	const FString& GetPreset() const { return Preset; }
	/** Server -> clients: every server.* value as "key=number" or "key=s:text". */
	TArray<FString> ExportServerSettings() const;
	void ImportServerSettings(const FString& InPreset, const TArray<FString>& Pairs);

	/** Loads any JSON file under the project's Data/ folder. Null on failure. */
	static TSharedPtr<FJsonObject> LoadDataFile(const FString& RelativePath);
	static FString DataDir();

private:
	FWRTuning();
	TSharedPtr<FJsonObject> Find(const FString& Path) const;
	TSharedPtr<FJsonObject> Root;
	mutable TSet<FString> WarnedKeys;
	TMap<FString, float> Overrides;
	TMap<FString, FString> StrOverrides;
	FString Preset;
	void ApplyServerPreset();
};

inline float WRTune(const TCHAR* Path, float Fallback) { return FWRTuning::Get().Num(Path, Fallback); }
