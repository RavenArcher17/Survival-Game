#pragma once

#include "CoreMinimal.h"

/** What the world list shows about one world (its info.json, plus the preset from its settings.json). */
struct FWRWorldInfo
{
	FString Id;            // its folder's name (never changes)
	FString Name;          // shown, renamable
	FString Preset;        // Data/server_presets.json key
	FDateTime Created;
	FDateTime LastPlayed;
	double PlaySeconds = 0.0;
	FString Version;       // the game version that last played it
};

/** What a world plays by: the preset it started from, and every value on the world settings screen (server.<key>). */
struct FWRWorldSettings
{
	FString Base;                    // a Data/server_presets.json key
	TMap<FString, float> Numbers;    // switches are 0 or 1
	FString DeathPenalty;
};

/**
 * Worlds: each is a folder, Saved/Worlds/<id>/, with everything the server keeps for it:
 *   info.json      name, created, last played, play time, version (only worlds with one are on the menu's list)
 *   settings.json  {"preset": ..., "base": ..., "settings": {...}} (what Saved/ServerDB/server.json was)
 *   world.json     buildings, tames, the clock (AWRGameMode::SaveWorld)
 *   clans.json     FWRClans
 *   players/       one file per character: a fresh character in each world
 * The game mode picks the world a game runs (?World=<id> from the menu's Host, -wrworld=<id> for tests and dedicated
 * servers, else the map's name without "L_"); the menu's own island (standalone, no world given) runs none and saves
 * nothing.
 */
namespace WRWorlds
{
	/** Saved/Worlds. */
	WILDREACH_API FString Root();
	WILDREACH_API FString Dir(const FString& Id);

	/** Server: the world this game runs ("" = none: nothing is loaded or saved). */
	WILDREACH_API const FString& Current();
	WILDREACH_API void SetCurrent(const FString& Id);
	/** Dir(Current()), or "" when there is none. */
	WILDREACH_API FString CurrentDir();

	/** 0.5 and before kept one save in Saved/ServerDB: once, it is copied into a world called Island (ServerDB stays as the backup). */
	WILDREACH_API void MigrateOldSave();

	/** Every world on the menu's list, last played first. */
	WILDREACH_API TArray<FWRWorldInfo> List();
	WILDREACH_API bool Read(const FString& Id, FWRWorldInfo& Out);
	/** A new world: its folder (named after it, made unique), info.json and settings.json. Returns its id. */
	WILDREACH_API FString Create(const FString& Name, const FWRWorldSettings& Settings);
	WILDREACH_API bool Rename(const FString& Id, const FString& NewName);
	/** Removes the world's folder and everything in it. */
	WILDREACH_API bool Delete(const FString& Id);
	/** Server: adds play time to the current world's info.json and marks it played now (worlds without one are left alone). */
	WILDREACH_API void Touch(double AddSeconds);

	/** A preset's values, with the defaults of what presets leave out (day length, hunger, creature numbers and difficulty). */
	WILDREACH_API FWRWorldSettings PresetSettings(const FString& Preset);
	/** What a world's settings.json says. */
	WILDREACH_API FWRWorldSettings ReadSettings(const FString& Id);
	/** settings.json: the preset alone, or "custom" with what differs from it. */
	WILDREACH_API bool WriteSettings(const FString& Id, const FWRWorldSettings& Settings);
	/** Anything changed from its preset. */
	WILDREACH_API bool IsCustom(const FWRWorldSettings& Settings);

	/** A world's name as a folder name: letters, digits, - and _. */
	WILDREACH_API FString SafeId(const FString& Name);
	/** Shown names for the presets: PvE, PvP, Casual co-op, Playtest. */
	WILDREACH_API FText PresetName(const FString& Preset);
	/** The presets a new world can start from, in the order the menu shows them. */
	WILDREACH_API const TArray<FString>& Presets();
}
