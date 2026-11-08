#include "WRWorlds.h"
#include "Wildreach.h"
#include "WRMainMenu.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRWorldsFiles
{
	FString GCurrent;
	bool bCurrentSet = false;

	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Obj;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Obj) || !Obj.IsValid())
		{
			return nullptr;
		}
		return Obj;
	}

	bool WriteJson(const FString& Path, const TSharedRef<FJsonObject>& Obj)
	{
		FString Text;
		FJsonSerializer::Serialize(Obj, TJsonWriterFactory<>::Create(&Text));
		return FFileHelper::SaveStringToFile(Text, *Path);
	}

	bool WriteInfo(const FWRWorldInfo& I)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("name"), I.Name);
		O->SetStringField(TEXT("created"), I.Created.ToIso8601());
		O->SetStringField(TEXT("lastPlayed"), I.LastPlayed.ToIso8601());
		O->SetNumberField(TEXT("playSeconds"), FMath::RoundToDouble(I.PlaySeconds));
		O->SetStringField(TEXT("version"), I.Version);
		return WriteJson(FPaths::Combine(WRWorlds::Dir(I.Id), TEXT("info.json")), O);
	}

	bool WritePreset(const FString& Dir, const FString& Preset)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("preset"), Preset);
		return WriteJson(FPaths::Combine(Dir, TEXT("settings.json")), O);
	}

	/** Every number the settings screen has. Not bounties or craftMultiplier: nothing in the game reads them yet. */
	const TCHAR* const RuleKeys[] = {
		TEXT("pvp"), TEXT("pvpInBossArenas"), TEXT("offlineProtection"), TEXT("clanSizeLimit"),
		TEXT("xpMultiplier"), TEXT("gatherMultiplier"), TEXT("tamingMultiplier"), TEXT("cropMultiplier"),
		TEXT("dayMinutes"), TEXT("hungerMultiplier"), TEXT("creatureNumbers"), TEXT("creatureDifficulty"),
		TEXT("upkeepMultiplier"), TEXT("bossRespawnHours"),
	};

	/** Reads what an object sets over S: a preset's values, or a custom world's changes. */
	void ReadRules(const TSharedPtr<FJsonObject>& O, FWRWorldSettings& S)
	{
		for (const TCHAR* Key : RuleKeys)
		{
			double Number = 0.0;
			bool bSwitch = false;
			if (O->TryGetNumberField(Key, Number)) S.Numbers.Add(Key, static_cast<float>(Number));
			else if (O->TryGetBoolField(Key, bSwitch)) S.Numbers.Add(Key, bSwitch ? 1.f : 0.f);
		}
		O->TryGetStringField(TEXT("deathPenalty"), S.DeathPenalty);
		O->TryGetStringField(TEXT("raids"), S.Raids);
	}

	/** The preset a server plays with no settings of its own (server_presets.json "default"). */
	FString DefaultPreset()
	{
		const TSharedPtr<FJsonObject> Presets = FWRTuning::LoadDataFile(TEXT("server_presets.json"));
		FString Preset = TEXT("pve");
		if (Presets.IsValid()) Presets->TryGetStringField(TEXT("default"), Preset);
		return Preset;
	}

	FDateTime ParseTime(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString Text;
		FDateTime Time;
		return O->TryGetStringField(Field, Text) && FDateTime::ParseIso8601(*Text, Time) ? Time : FDateTime(0);
	}
}

FString WRWorlds::Root()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Worlds"));
}

FString WRWorlds::Dir(const FString& Id)
{
	return FPaths::Combine(Root(), Id);
}

const FString& WRWorlds::Current()
{
	using namespace WRWorldsFiles;
	// Until a game mode sets it (the tuning's first load on a dedicated server comes earlier): -wrworld=.
	if (!bCurrentSet)
	{
		bCurrentSet = true;
		FString Id;
		FParse::Value(FCommandLine::Get(), TEXT("-wrworld="), Id);
		GCurrent = SafeId(Id);
	}
	return GCurrent;
}

void WRWorlds::SetCurrent(const FString& Id)
{
	WRWorldsFiles::GCurrent = SafeId(Id);
	WRWorldsFiles::bCurrentSet = true;
}

FString WRWorlds::CurrentDir()
{
	return Current().IsEmpty() ? FString() : Dir(Current());
}

void WRWorlds::MigrateOldSave()
{
	using namespace WRWorldsFiles;
	static bool bChecked = false;
	if (bChecked)
	{
		return;
	}
	bChecked = true;
	IFileManager& FM = IFileManager::Get();
	const FString Old = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ServerDB"));
	const FString Marker = FPaths::Combine(Root(), TEXT("moved_from_ServerDB.txt"));
	const FString OldWorld = FPaths::Combine(Old, TEXT("world_L_Island.json"));
	TArray<FString> Players;
	FM.FindFiles(Players, *FPaths::Combine(Old, TEXT("players"), TEXT("*.json")), true, false);
	// Nothing played before worlds (or only the tests' arenas), or moved already.
	if (FM.FileExists(*Marker) || (!FM.FileExists(*OldWorld) && Players.Num() == 0))
	{
		return;
	}
	const FString To = Dir(TEXT("Island"));
	if (!FM.DirectoryExists(*To))
	{
		FM.MakeDirectory(*FPaths::Combine(To, TEXT("players")), true);
		bool bOk = true;
		const auto Copy = [&FM, &bOk](const FString& From, const FString& Dest)
		{
			if (FM.FileExists(*From)) bOk &= FM.Copy(*Dest, *From) == COPY_OK;
		};
		Copy(OldWorld, FPaths::Combine(To, TEXT("world.json")));
		Copy(FPaths::Combine(Old, TEXT("clans.json")), FPaths::Combine(To, TEXT("clans.json")));
		FDateTime Oldest = FDateTime::UtcNow(), Newest = FDateTime(0);
		const auto Saw = [&Oldest, &Newest](const FDateTime& Time)
		{
			Oldest = FMath::Min(Oldest, Time);
			Newest = FMath::Max(Newest, Time);
		};
		if (FM.FileExists(*OldWorld)) Saw(FM.GetTimeStamp(*OldWorld));
		for (const FString& P : Players)
		{
			const FString From = FPaths::Combine(Old, TEXT("players"), P);
			Copy(From, FPaths::Combine(To, TEXT("players"), P));
			Saw(FM.GetTimeStamp(*From));
		}
		// The old server.json (a custom preset) becomes the world's settings; without one it played the file's default.
		const FString OldSettings = FPaths::Combine(Old, TEXT("server.json"));
		if (FM.FileExists(*OldSettings)) Copy(OldSettings, FPaths::Combine(To, TEXT("settings.json")));
		else bOk &= WritePreset(To, DefaultPreset());
		FWRWorldInfo Info;
		Info.Id = Info.Name = TEXT("Island");
		// Played after it was made, however few files it has: the list says when, and "not played yet" would be wrong.
		Info.Created = Oldest;
		Info.LastPlayed = FMath::Max(Newest, Oldest + FTimespan::FromSeconds(1.0));
		bOk &= WriteInfo(Info);
		UE_LOG(LogWildreach, Log, TEXT("WORLDS copied the old save (Saved/ServerDB: %d characters) into the world 'Island'"), Players.Num());
		if (!bOk)
		{
			UE_LOG(LogWildreach, Warning, TEXT("WORLDS couldn't copy all of Saved/ServerDB into Island; ServerDB is untouched"));
		}
	}
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("Saved/ServerDB was copied into the world Island (Saved/Worlds/Island) on %s.\r\nServerDB is kept as a backup and is no longer used.\r\n"),
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"))), *Marker);
}

TArray<FWRWorldInfo> WRWorlds::List()
{
	MigrateOldSave();
	TArray<FString> Ids;
	IFileManager::Get().FindFiles(Ids, *FPaths::Combine(Root(), TEXT("*")), false, true);
	TArray<FWRWorldInfo> Out;
	for (const FString& Id : Ids)
	{
		FWRWorldInfo Info;
		if (Read(Id, Info)) Out.Add(Info);
	}
	Out.Sort([](const FWRWorldInfo& A, const FWRWorldInfo& B) { return A.LastPlayed > B.LastPlayed; });
	return Out;
}

bool WRWorlds::Read(const FString& Id, FWRWorldInfo& Out)
{
	using namespace WRWorldsFiles;
	const TSharedPtr<FJsonObject> Info = ReadJson(FPaths::Combine(Dir(Id), TEXT("info.json")));
	if (Id.IsEmpty() || !Info.IsValid())
	{
		return false;  // a test's or a dedicated server's world (-wrworld): not on the list
	}
	Out = FWRWorldInfo();
	Out.Id = Id;
	if (!Info->TryGetStringField(TEXT("name"), Out.Name) || Out.Name.IsEmpty()) Out.Name = Id;
	Out.Created = ParseTime(Info, TEXT("created"));
	Out.LastPlayed = ParseTime(Info, TEXT("lastPlayed"));
	Info->TryGetNumberField(TEXT("playSeconds"), Out.PlaySeconds);
	Info->TryGetStringField(TEXT("version"), Out.Version);
	const TSharedPtr<FJsonObject> Settings = ReadJson(FPaths::Combine(Dir(Id), TEXT("settings.json")));
	if (!Settings.IsValid() || !Settings->TryGetStringField(TEXT("preset"), Out.Preset)) Out.Preset = DefaultPreset();
	return true;
}

FWRWorldSettings WRWorlds::PresetSettings(const FString& Preset)
{
	FWRWorldSettings S;
	S.Base = Preset;
	// The presets all play these the same way, so they don't list them.
	S.Numbers.Add(TEXT("dayMinutes"), WRTune(TEXT("clock.dayMinutes"), 60.f));
	S.Numbers.Add(TEXT("hungerMultiplier"), 1.f);
	S.Numbers.Add(TEXT("creatureNumbers"), 1.f);
	S.Numbers.Add(TEXT("creatureDifficulty"), 1.f);
	const TSharedPtr<FJsonObject> Presets = FWRTuning::LoadDataFile(TEXT("server_presets.json"));
	const TSharedPtr<FJsonObject>* List = nullptr;
	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (Presets.IsValid() && Presets->TryGetObjectField(TEXT("presets"), List) && (*List)->TryGetObjectField(Preset, Values))
	{
		WRWorldsFiles::ReadRules(*Values, S);
	}
	return S;
}

FWRWorldSettings WRWorlds::ReadSettings(const FString& Id)
{
	const TSharedPtr<FJsonObject> File = WRWorldsFiles::ReadJson(FPaths::Combine(Dir(Id), TEXT("settings.json")));
	FString Preset = WRWorldsFiles::DefaultPreset();
	if (File.IsValid()) File->TryGetStringField(TEXT("preset"), Preset);
	if (Preset != TEXT("custom"))
	{
		return PresetSettings(Preset);
	}
	FString Base = TEXT("pve");
	File->TryGetStringField(TEXT("base"), Base);
	FWRWorldSettings S = PresetSettings(Base);
	const TSharedPtr<FJsonObject>* Changes = nullptr;
	if (File->TryGetObjectField(TEXT("settings"), Changes))
	{
		WRWorldsFiles::ReadRules(*Changes, S);
	}
	return S;
}

bool WRWorlds::WriteSettings(const FString& Id, const FWRWorldSettings& Settings)
{
	const FWRWorldSettings Preset = PresetSettings(Settings.Base);
	const TSharedRef<FJsonObject> Changes = MakeShared<FJsonObject>();
	for (const TPair<FString, float>& KV : Settings.Numbers)
	{
		const float* Was = Preset.Numbers.Find(KV.Key);
		if (!Was || !FMath::IsNearlyEqual(*Was, KV.Value, 0.001f)) Changes->SetNumberField(KV.Key, KV.Value);
	}
	if (Settings.DeathPenalty != Preset.DeathPenalty) Changes->SetStringField(TEXT("deathPenalty"), Settings.DeathPenalty);
	if (Settings.Raids != Preset.Raids) Changes->SetStringField(TEXT("raids"), Settings.Raids);
	if (Changes->Values.Num() == 0)
	{
		return WRWorldsFiles::WritePreset(Dir(Id), Settings.Base);
	}
	const TSharedRef<FJsonObject> File = MakeShared<FJsonObject>();
	File->SetStringField(TEXT("preset"), TEXT("custom"));
	File->SetStringField(TEXT("base"), Settings.Base);
	File->SetObjectField(TEXT("settings"), Changes);
	return WRWorldsFiles::WriteJson(FPaths::Combine(Dir(Id), TEXT("settings.json")), File);
}

bool WRWorlds::IsCustom(const FWRWorldSettings& Settings)
{
	const FWRWorldSettings Preset = PresetSettings(Settings.Base);
	for (const TPair<FString, float>& KV : Settings.Numbers)
	{
		const float* Was = Preset.Numbers.Find(KV.Key);
		if (!Was || !FMath::IsNearlyEqual(*Was, KV.Value, 0.001f)) return true;
	}
	return Settings.DeathPenalty != Preset.DeathPenalty || Settings.Raids != Preset.Raids;
}

FString WRWorlds::Create(const FString& Name, const FWRWorldSettings& Settings)
{
	using namespace WRWorldsFiles;
	const FString Shown = Name.TrimStartAndEnd().Left(32);
	FString Base = SafeId(Shown);
	if (Base.IsEmpty()) Base = TEXT("World");
	FString Id = Base;
	for (int32 N = 2; IFileManager::Get().DirectoryExists(*Dir(Id)); ++N)
	{
		Id = FString::Printf(TEXT("%s_%d"), *Base, N);
	}
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Dir(Id), TEXT("players")), true);
	FWRWorldInfo Info;
	Info.Id = Id;
	Info.Name = Shown.IsEmpty() ? Id : Shown;
	Info.Created = Info.LastPlayed = FDateTime::UtcNow();
	Info.Version = SWRMainMenu::GameVersion();
	if (!WriteSettings(Id, Settings) || !WriteInfo(Info))
	{
		UE_LOG(LogWildreach, Warning, TEXT("WORLDS couldn't make the world '%s' in %s"), *Info.Name, *Dir(Id));
		return FString();
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLDS made '%s' (%s%s) in Saved/Worlds/%s"), *Info.Name, *Settings.Base, IsCustom(Settings) ? TEXT(", changed") : TEXT(""), *Id);
	return Id;
}

bool WRWorlds::Rename(const FString& Id, const FString& NewName)
{
	FWRWorldInfo Info;
	const FString Shown = NewName.TrimStartAndEnd().Left(32);
	if (Shown.IsEmpty() || !Read(Id, Info))
	{
		return false;
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLDS renamed '%s' to '%s'"), *Info.Name, *Shown);
	Info.Name = Shown;
	return WRWorldsFiles::WriteInfo(Info);
}

bool WRWorlds::Delete(const FString& Id)
{
	// Only a world folder itself: never Saved/Worlds, or anything a strange id could reach.
	if (Id.IsEmpty() || SafeId(Id) != Id || !IFileManager::Get().DirectoryExists(*Dir(Id)))
	{
		return false;
	}
	const bool bOk = IFileManager::Get().DeleteDirectory(*Dir(Id), false, true);
	UE_LOG(LogWildreach, Log, TEXT("WORLDS deleted Saved/Worlds/%s%s"), *Id, bOk ? TEXT("") : TEXT(" (failed)"));
	return bOk;
}

void WRWorlds::Touch(double AddSeconds)
{
	FWRWorldInfo Info;
	if (!Read(Current(), Info))
	{
		return;
	}
	Info.PlaySeconds += FMath::Max(0.0, AddSeconds);
	Info.LastPlayed = FDateTime::UtcNow();
	Info.Version = SWRMainMenu::GameVersion();
	WRWorldsFiles::WriteInfo(Info);
}

FString WRWorlds::SafeId(const FString& Name)
{
	FString Out;
	for (const TCHAR C : Name.TrimStartAndEnd())
	{
		if ((C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('-') || C == TEXT('_'))
		{
			Out.AppendChar(C);
		}
		else if (C == TEXT(' ') && !Out.IsEmpty() && !Out.EndsWith(TEXT("_")))
		{
			Out.AppendChar(TEXT('_'));
		}
	}
	Out.LeftInline(32);
	while (Out.EndsWith(TEXT("_"))) Out.LeftChopInline(1);
	return Out;
}

FText WRWorlds::PresetName(const FString& Preset)
{
	if (Preset == TEXT("pve")) return LOCTEXT("PresetPvE", "PvE");
	if (Preset == TEXT("pvp_standard")) return LOCTEXT("PresetPvP", "PvP");
	if (Preset == TEXT("casual_coop")) return LOCTEXT("PresetCasual", "Casual co-op");
	if (Preset == TEXT("playtest")) return LOCTEXT("PresetPlaytest", "Playtest");
	if (Preset == TEXT("custom")) return LOCTEXT("PresetCustom", "Custom");
	return FText::FromString(Preset);
}

const TArray<FString>& WRWorlds::Presets()
{
	static const TArray<FString> List = { TEXT("pve"), TEXT("pvp_standard"), TEXT("casual_coop"), TEXT("playtest") };
	return List;
}

#undef LOCTEXT_NAMESPACE
