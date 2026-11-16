#pragma once

#include "CoreMinimal.h"

/**
 * The world store (M7 1; cooks/build-server-raids.md 2.1): everything the server keeps for a world, in one SQLite file,
 * Saved/Worlds/<id>/world.db (WAL, exclusive: one server per world folder).
 *   meta        world id, when it was made, when its JSON files were brought in
 *   accounts    who owns each name (WRIdentity: a key hash, a platform id once Steam/EOS is on)
 *   characters  one row per character: the character's JSON (paths, inventory, skills...: the "inventory blob")
 *   world_docs  one row per world system: world (pieces, tames, eggs, clock), clans, wildlife, portals, world_systems, raid
 *   backups     the copies on disk (Saved/Worlds/<id>/backups/), for their rotation
 * Tables come from Data/db/migrations/NNNN_*.sql, applied in order past PRAGMA user_version at open.
 *
 * Writes are batched: the game thread hands over the text and a writer thread commits everything pending in one
 * transaction every store.flushSeconds (and at once on Flush(true): logout, shutdown). Reads see pending writes.
 * Backups are VACUUM INTO snapshots: hourly (keep store.hourlyKeep), daily (store.dailyKeep), on clean shutdown
 * (store.shutdownKeep) and once before the JSON import (kept).
 *
 * A world that still has its JSON files (0.7 and before) has them imported on first open, in one transaction; the files
 * move to json-before-sqlite/ and are never deleted. info.json and settings.json stay files (the menu reads them).
 * -wrstore=json keeps the JSON files instead (Raven's headless test tools read and delete them).
 * With no world (the menu's island) every call does nothing.
 */
namespace WRStore
{
	/** Server: open the current world's store (WRWorlds::Current). Imports the JSON files once. Closes itself the tick
	 *  after World is cleaned up (and on the module's shutdown). */
	WILDREACH_API void Open(class UWorld* World);
	/** Server: commit what's pending, take the shutdown backup, close. */
	WILDREACH_API void Close();
	/** True when the world runs on SQLite (false: JSON files, or no world). */
	WILDREACH_API bool IsSql();

	/** A world document (world, clans, wildlife, portals, world_systems, raid): false if there's none. */
	WILDREACH_API bool ReadDoc(const FString& Kind, FString& OutText);
	WILDREACH_API void WriteDoc(const FString& Kind, const FString& Text);
	/** Whether this world has been played (it has a world document): wildlife starts over with a world that hasn't. */
	WILDREACH_API bool HasDoc(const FString& Kind);

	/** A character's save, by name (case-insensitive). */
	WILDREACH_API bool ReadCharacter(const FString& Name, FString& OutText);
	WILDREACH_API void WriteCharacter(const FString& Name, const FString& MainPath, const FString& Text);

	/** Who owns a name on this world (WRIdentity). */
	struct FAccount
	{
		FString Name;
		FString KeyHash;
		FString Platform;
		FString PlatformId;
		int64 FirstSeen = 0;
		int64 LastSeen = 0;
		FString LastAddress;
	};
	WILDREACH_API bool ReadAccount(const FString& Name, FAccount& Out);
	WILDREACH_API void WriteAccount(const FAccount& Account);

	/** Commit what's pending now (bWait: and wait until it's on disk). */
	WILDREACH_API void Flush(bool bWait);
	/** Server, once a minute: takes the hourly (and daily) backup when one is due. */
	WILDREACH_API void Tick();
	/** "": fine; else what went wrong at open (a damaged file restored from a backup...), for the host's banner. */
	WILDREACH_API const FString& OpenNote();

	/** One snapshot in a world's backups/ folder (cooks/cookpass-multiplayer.md 3.8). Kind: hourly, daily, shutdown,
	 *  pre-import, before-restore. */
	struct FBackupInfo
	{
		FString File;
		FString Kind;
		FDateTime MadeUtc;
		int64 Bytes = 0;
	};
	/** A world's snapshots, newest first (read from the folder: works while the world isn't running). */
	WILDREACH_API TArray<FBackupInfo> ListBackups(const FString& WorldId);
	/** Puts a snapshot back as the world's world.db, keeping the world as it was as backups/before-restore-<time>.db
	 *  first. Refused while this game has that world open. False with the reason. */
	WILDREACH_API bool RestoreBackup(const FString& WorldId, const FString& File, FString& OutWhy);
}
