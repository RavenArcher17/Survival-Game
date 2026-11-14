// The world store: one SQLite file per world, batched writes on a writer thread, rolling backups, the one-time JSON
// import (WRStore.h; cooks/build-server-raids.md 2.1).
#include "WRStore.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "HAL/FileManager.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include <atomic>
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

namespace WRStoreImpl
{
	enum class EKind : uint8 { Doc, Character, Account };
	class FWriter;

	struct FPending
	{
		EKind Kind = EKind::Doc;
		FString Key;
		FString Text;
		FString MainPath;
		WRStore::FAccount Account;
		int64 Utc = 0;
	};

	int64 UtcNow() { return FDateTime::UtcNow().ToUnixTimestamp(); }

	/** Everything the open world's store holds. One connection (the engine's SQLite runs one per file), behind DbLock. */
	struct FState
	{
		FString WorldId;
		FString Dir;
		bool bSql = false;
		bool bOpen = false;
		TUniquePtr<FSQLiteDatabase> Db;
		FCriticalSection DbLock;
		FCriticalSection PendingLock;
		TMap<FString, FPending> Pending;     // newest text per row, waiting for the writer
		TMap<FString, FPending> InFlight;    // being committed now (reads still see it)
		TArray<FString> BackupRequests;      // kinds to snapshot after the next commit
		FString Note;
		int64 LastHourly = 0;
		int32 LastDailyDay = -1;
		FWriter* Writer = nullptr;
		FRunnableThread* Thread = nullptr;
		/** The world whose game opened the store: when it's cleaned up (its subsystems' last saves done), it closes. */
		TWeakObjectPtr<UWorld> World;
		FDelegateHandle CleanupHandle;
		/** Counts opens: a close queued for an old world's cleanup leaves a store opened since alone. */
		uint32 Generation = 0;
		/** Flush(true) asks for this sequence number and waits until the writer has committed it. */
		std::atomic<uint64> RequestSeq { 0 };
		std::atomic<uint64> CommittedSeq { 0 };
	};
	FState G;

	FString RowKey(EKind Kind, const FString& Key)
	{
		return FString::Printf(TEXT("%d:%s"), int32(Kind), *Key.ToLower());
	}

	bool Exec(FSQLiteDatabase& Db, const TCHAR* Sql)
	{
		if (Db.Execute(Sql)) return true;
		UE_LOG(LogWildreach, Error, TEXT("STORE sql failed (%s): %s"), *Db.GetLastError(), Sql);
		return false;
	}

	/** Statements of a migration file: comments dropped, split at ';' (the files hold no strings with ';' in them). */
	TArray<FString> SplitSql(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		FString Clean;
		for (FString L : Lines)
		{
			int32 Dash = INDEX_NONE;
			if (L.FindChar(TEXT('-'), Dash) && L.Mid(Dash, 2) == TEXT("--")) L.LeftInline(Dash);
			Clean += L + TEXT("\n");
		}
		TArray<FString> Out;
		TArray<FString> Parts;
		Clean.ParseIntoArray(Parts, TEXT(";"), true);
		for (FString& P : Parts)
		{
			P.TrimStartAndEndInline();
			if (!P.IsEmpty()) Out.Add(P);
		}
		return Out;
	}

	/** Data/db/migrations/NNNN_*.sql past user_version, each in its own transaction. */
	bool Migrate(FSQLiteDatabase& Db)
	{
		int32 Version = 0;
		Db.GetUserVersion(Version);
		const FString MigDir = FPaths::Combine(FWRTuning::DataDir(), TEXT("db"), TEXT("migrations"));
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(MigDir, TEXT("*.sql")), true, false);
		Files.Sort();
		if (Files.Num() == 0)
		{
			UE_LOG(LogWildreach, Error, TEXT("STORE no migrations in %s: the world can't be stored in SQLite"), *MigDir);
			return false;
		}
		for (const FString& File : Files)
		{
			const int32 Number = FCString::Atoi(*File.Left(4));
			if (Number <= Version) continue;
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(MigDir, File)))
			{
				UE_LOG(LogWildreach, Error, TEXT("STORE can't read migration %s"), *File);
				return false;
			}
			bool bOk = Exec(Db, TEXT("BEGIN IMMEDIATE"));
			for (const FString& Statement : SplitSql(Text))
			{
				bOk = bOk && Exec(Db, *Statement);
			}
			bOk = bOk && Db.SetUserVersion(Number);
			if (!bOk || !Exec(Db, TEXT("COMMIT")))
			{
				Db.Execute(TEXT("ROLLBACK"));
				UE_LOG(LogWildreach, Error, TEXT("STORE migration %s failed: %s"), *File, *Db.GetLastError());
				return false;
			}
			UE_LOG(LogWildreach, Log, TEXT("STORE migrated to %d (%s)"), Number, *File);
			Version = Number;
		}
		return true;
	}

	FString BackupDir() { return FPaths::Combine(G.Dir, TEXT("backups")); }
	FString DbPath() { return FPaths::Combine(G.Dir, TEXT("world.db")); }

	/** Writes one pending row (inside the writer's transaction). */
	bool WriteRow(FSQLiteDatabase& Db, const FPending& P)
	{
		FSQLitePreparedStatement S;
		if (P.Kind == EKind::Doc)
		{
			S = Db.PrepareStatement(TEXT("INSERT INTO world_docs(kind, saved_utc, body) VALUES(?1, ?2, ?3) ON CONFLICT(kind) DO UPDATE SET saved_utc = excluded.saved_utc, body = excluded.body"));
			if (!S.IsValid()) return false;
			S.SetBindingValueByIndex(1, P.Key);
			S.SetBindingValueByIndex(2, P.Utc);
			S.SetBindingValueByIndex(3, P.Text);
		}
		else if (P.Kind == EKind::Character)
		{
			S = Db.PrepareStatement(TEXT("INSERT INTO characters(name, main_path, saved_utc, body) VALUES(?1, ?2, ?3, ?4) ON CONFLICT(name) DO UPDATE SET main_path = excluded.main_path, saved_utc = excluded.saved_utc, body = excluded.body"));
			if (!S.IsValid()) return false;
			S.SetBindingValueByIndex(1, P.Key);
			S.SetBindingValueByIndex(2, P.MainPath);
			S.SetBindingValueByIndex(3, P.Utc);
			S.SetBindingValueByIndex(4, P.Text);
		}
		else
		{
			const WRStore::FAccount& A = P.Account;
			S = Db.PrepareStatement(TEXT("INSERT INTO accounts(name, key_hash, platform, platform_id, first_seen, last_seen, last_address) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7) ")
				TEXT("ON CONFLICT(name) DO UPDATE SET key_hash = excluded.key_hash, platform = excluded.platform, platform_id = excluded.platform_id, last_seen = excluded.last_seen, last_address = excluded.last_address"));
			if (!S.IsValid()) return false;
			S.SetBindingValueByIndex(1, A.Name);
			S.SetBindingValueByIndex(2, A.KeyHash);
			S.SetBindingValueByIndex(3, A.Platform);
			S.SetBindingValueByIndex(4, A.PlatformId);
			S.SetBindingValueByIndex(5, A.FirstSeen);
			S.SetBindingValueByIndex(6, A.LastSeen);
			S.SetBindingValueByIndex(7, A.LastAddress);
		}
		return S.Execute();
	}

	/** Commits everything pending in one transaction. Rows that fail stay pending (unless newer text came meanwhile). */
	void CommitPending()
	{
		{
			FScopeLock Lock(&G.PendingLock);
			if (G.Pending.Num() == 0) return;
			G.InFlight = MoveTemp(G.Pending);
			G.Pending.Reset();
		}
		bool bOk = false;
		{
			FScopeLock Lock(&G.DbLock);
			if (G.Db.IsValid() && G.Db->IsValid())
			{
				bOk = Exec(*G.Db, TEXT("BEGIN IMMEDIATE"));
				for (const TPair<FString, FPending>& KV : G.InFlight)
				{
					if (!bOk) break;
					bOk = WriteRow(*G.Db, KV.Value);
					if (!bOk) UE_LOG(LogWildreach, Error, TEXT("STORE couldn't write %s: %s"), *KV.Key, *G.Db->GetLastError());
				}
				bOk = bOk && Exec(*G.Db, TEXT("COMMIT"));
				if (!bOk) G.Db->Execute(TEXT("ROLLBACK"));
			}
		}
		FScopeLock Lock(&G.PendingLock);
		if (!bOk)
		{
			// Kept for the next flush (disk full, a locked file): never dropped.
			for (TPair<FString, FPending>& KV : G.InFlight)
			{
				if (!G.Pending.Contains(KV.Key)) G.Pending.Add(KV.Key, MoveTemp(KV.Value));
			}
			UE_LOG(LogWildreach, Error, TEXT("STORE commit failed: %d rows stay pending and are tried again"), G.Pending.Num());
		}
		G.InFlight.Reset();
	}

	int32 KeepFor(const FString& Kind)
	{
		if (Kind == TEXT("hourly")) return FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("store.hourlyKeep"), 24.f)));
		if (Kind == TEXT("daily")) return FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("store.dailyKeep"), 7.f)));
		if (Kind == TEXT("shutdown")) return FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("store.shutdownKeep"), 5.f)));
		return 0;   // import: kept forever
	}

	/** A consistent snapshot of the DB (VACUUM INTO), then the oldest of its kind past the keep count go. Writer thread. */
	void MakeBackup(const FString& Kind)
	{
		FScopeLock Lock(&G.DbLock);
		if (!G.Db.IsValid() || !G.Db->IsValid()) return;
		IFileManager& FM = IFileManager::Get();
		FM.MakeDirectory(*BackupDir(), true);
		const FString File = FString::Printf(TEXT("%s-%s.db"), *Kind, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
		const FString Full = FPaths::ConvertRelativePathToFull(FPaths::Combine(BackupDir(), File));
		FString Quoted = Full.Replace(TEXT("'"), TEXT("''"));
		if (!Exec(*G.Db, *FString::Printf(TEXT("VACUUM INTO '%s'"), *Quoted)))
		{
			UE_LOG(LogWildreach, Error, TEXT("STORE %s backup failed"), *Kind);
			return;
		}
		const int64 Bytes = FM.FileSize(*Full);
		FSQLitePreparedStatement Add = G.Db->PrepareStatement(TEXT("INSERT OR REPLACE INTO backups(file, kind, made_utc, bytes) VALUES(?1, ?2, ?3, ?4)"));
		if (Add.IsValid())
		{
			Add.SetBindingValueByIndex(1, File);
			Add.SetBindingValueByIndex(2, Kind);
			Add.SetBindingValueByIndex(3, UtcNow());
			Add.SetBindingValueByIndex(4, Bytes);
			Add.Execute();
		}
		UE_LOG(LogWildreach, Log, TEXT("STORE %s backup: backups/%s (%lld KB)"), *Kind, *File, Bytes / 1024);
		const int32 Keep = KeepFor(Kind);
		if (Keep <= 0) return;
		TArray<FString> Old;
		FSQLitePreparedStatement List = G.Db->PrepareStatement(TEXT("SELECT file FROM backups WHERE kind = ?1 ORDER BY made_utc DESC"));
		if (!List.IsValid()) return;
		List.SetBindingValueByIndex(1, Kind);
		int32 Seen = 0;
		while (List.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			FString Name;
			List.GetColumnValueByIndex(0, Name);
			if (++Seen > Keep) Old.Add(Name);
		}
		List.Destroy();
		for (const FString& Name : Old)
		{
			FM.Delete(*FPaths::Combine(BackupDir(), Name), false, true, true);
			FSQLitePreparedStatement Drop = G.Db->PrepareStatement(TEXT("DELETE FROM backups WHERE file = ?1"));
			if (Drop.IsValid())
			{
				Drop.SetBindingValueByIndex(1, Name);
				Drop.Execute();
			}
		}
	}

	void RunBackups()
	{
		TArray<FString> Kinds;
		{
			FScopeLock Lock(&G.PendingLock);
			Kinds = MoveTemp(G.BackupRequests);
			G.BackupRequests.Reset();
		}
		for (const FString& K : Kinds) MakeBackup(K);
	}

	/** The writer: commits every store.flushSeconds, or at once when woken. */
	class FWriter : public FRunnable
	{
	public:
		FEvent* Wake = FPlatformProcess::GetSynchEventFromPool(false);
		std::atomic<bool> bStop { false };
		uint32 Ms = 10000;
		virtual ~FWriter() override
		{
			FPlatformProcess::ReturnSynchEventToPool(Wake);
		}
		void Pass()
		{
			const uint64 Asked = G.RequestSeq.load();
			CommitPending();
			RunBackups();
			G.CommittedSeq.store(Asked);
		}
		virtual uint32 Run() override
		{
			while (!bStop)
			{
				Wake->Wait(Ms);
				Pass();
			}
			Pass();
			return 0;
		}
		virtual void Stop() override { bStop = true; Wake->Trigger(); }
	};

	bool QueryText(const TCHAR* Sql, const FString& Key, FString& Out)
	{
		FScopeLock Lock(&G.DbLock);
		if (!G.Db.IsValid() || !G.Db->IsValid()) return false;
		FSQLitePreparedStatement S = G.Db->PrepareStatement(Sql);
		if (!S.IsValid()) return false;
		S.SetBindingValueByIndex(1, Key);
		if (S.Step() != ESQLitePreparedStatementStepResult::Row) return false;
		return S.GetColumnValueByIndex(0, Out);
	}

	bool PendingText(EKind Kind, const FString& Key, FString& Out, const FPending** OutRow = nullptr)
	{
		FScopeLock Lock(&G.PendingLock);
		const FString K = RowKey(Kind, Key);
		const FPending* P = G.Pending.Find(K);
		if (!P) P = G.InFlight.Find(K);
		if (!P) return false;
		Out = P->Text;
		if (OutRow) *OutRow = P;
		return true;
	}

	void Queue(FPending&& P)
	{
		P.Utc = UtcNow();
		const FString K = RowKey(P.Kind, P.Key);
		FScopeLock Lock(&G.PendingLock);
		G.Pending.Add(K, MoveTemp(P));
	}

	void SetMeta(const FString& Key, const FString& Value)
	{
		FScopeLock Lock(&G.DbLock);
		FSQLitePreparedStatement S = G.Db->PrepareStatement(TEXT("INSERT OR REPLACE INTO meta(key, value) VALUES(?1, ?2)"));
		if (!S.IsValid()) return;
		S.SetBindingValueByIndex(1, Key);
		S.SetBindingValueByIndex(2, Value);
		S.Execute();
	}

	FString GetMeta(const FString& Key)
	{
		FString V;
		QueryText(TEXT("SELECT value FROM meta WHERE key = ?1"), Key, V);
		return V;
	}

	FString JsonFileFor(EKind Kind, const FString& Key)
	{
		if (Kind == EKind::Doc) return FPaths::Combine(G.Dir, Key + TEXT(".json"));
		FString Safe;
		for (const TCHAR C : Key) Safe.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') ? C : TEXT('_'));
		return FPaths::Combine(G.Dir, TEXT("players"), Safe + TEXT(".json"));
	}

	/** The world's 0.7 JSON files into the DB in one transaction (no wipe); then they move to json-before-sqlite/. */
	void ImportJson()
	{
		if (!GetMeta(TEXT("imported_from_json_utc")).IsEmpty()) return;
		IFileManager& FM = IFileManager::Get();
		static const TCHAR* Docs[] = { TEXT("world"), TEXT("clans"), TEXT("wildlife"), TEXT("portals"), TEXT("world_systems") };
		TArray<FString> Moved;
		TArray<FPending> Rows;
		for (const TCHAR* Doc : Docs)
		{
			const FString Path = FPaths::Combine(G.Dir, FString(Doc) + TEXT(".json"));
			FString Text;
			if (FM.FileExists(*Path) && FFileHelper::LoadFileToString(Text, *Path))
			{
				FPending P;
				P.Kind = EKind::Doc; P.Key = Doc; P.Text = Text; P.Utc = FM.GetTimeStamp(*Path).ToUnixTimestamp();
				Rows.Add(MoveTemp(P));
				Moved.Add(FString(Doc) + TEXT(".json"));
			}
		}
		TArray<FString> Players;
		FM.FindFiles(Players, *FPaths::Combine(G.Dir, TEXT("players"), TEXT("*.json")), true, false);
		for (const FString& File : Players)
		{
			const FString Path = FPaths::Combine(G.Dir, TEXT("players"), File);
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path)) continue;
			// The name inside the file (the file's name had its odd letters replaced).
			FString Name = FPaths::GetBaseFilename(File);
			const int32 At = Text.Find(TEXT("\"name\""));
			if (At != INDEX_NONE)
			{
				const int32 Q1 = Text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Text.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At) + 1);
				const int32 Q2 = Q1 != INDEX_NONE ? Text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Q1 + 1) : INDEX_NONE;
				if (Q1 != INDEX_NONE && Q2 > Q1 + 1) Name = Text.Mid(Q1 + 1, Q2 - Q1 - 1);
			}
			FPending P;
			P.Kind = EKind::Character; P.Key = Name; P.Text = Text; P.Utc = FM.GetTimeStamp(*Path).ToUnixTimestamp();
			Rows.Add(MoveTemp(P));
			Moved.Add(FString(TEXT("players/")) + File);
		}
		if (Rows.Num() > 0)
		{
			// The empty DB as it was before the import: a known-good point to come back to.
			{
				FScopeLock Lock(&G.PendingLock);
				G.BackupRequests.Add(TEXT("import"));
			}
			RunBackups();
			bool bOk = false;
			{
				FScopeLock Lock(&G.DbLock);
				bOk = Exec(*G.Db, TEXT("BEGIN IMMEDIATE"));
				for (const FPending& P : Rows) bOk = bOk && WriteRow(*G.Db, P);
				bOk = bOk && Exec(*G.Db, TEXT("COMMIT"));
				if (!bOk) G.Db->Execute(TEXT("ROLLBACK"));
			}
			if (!bOk)
			{
				// Nothing moved: the files stay where they are and the import is tried again next start.
				UE_LOG(LogWildreach, Error, TEXT("STORE import of the JSON saves failed; they're untouched and will be tried again"));
				G.Note = TEXT("The world's old save files couldn't be brought into the new store; they're untouched.");
				return;
			}
			const FString Keep = FPaths::Combine(G.Dir, TEXT("json-before-sqlite"));
			FM.MakeDirectory(*FPaths::Combine(Keep, TEXT("players")), true);
			for (const FString& Rel : Moved)
			{
				FM.Move(*FPaths::Combine(Keep, Rel), *FPaths::Combine(G.Dir, Rel), true, true);
			}
			FFileHelper::SaveStringToFile(FString::Printf(TEXT("These were this world's save files until %s UTC, when they were brought into world.db (Wildreach 0.8 store).\r\nThey're kept as they were and no longer read.\r\n"),
				*FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M"))), *FPaths::Combine(Keep, TEXT("README.txt")));
			UE_LOG(LogWildreach, Log, TEXT("STORE imported %d JSON saves (%d characters) into world.db; the files are in json-before-sqlite/"), Rows.Num(), Players.Num());
		}
		SetMeta(TEXT("imported_from_json_utc"), LexToString(UtcNow()));
	}

	/** quick_check failed: put the damaged file aside and bring back the newest backup that passes. */
	bool RestoreNewestBackup()
	{
		IFileManager& FM = IFileManager::Get();
		const FString Bad = DbPath() + TEXT(".bad-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
		FM.Move(*Bad, *DbPath(), true, true);
		FM.Delete(*(DbPath() + TEXT("-wal")), false, true, true);
		FM.Delete(*(DbPath() + TEXT("-shm")), false, true, true);
		TArray<FString> Files;
		FM.FindFiles(Files, *FPaths::Combine(BackupDir(), TEXT("*.db")), true, false);
		// Newest first by the time in the name (kind-yyyymmdd-hhmmss).
		Files.Sort([](const FString& A, const FString& B) { return A.Right(18) > B.Right(18); });
		for (const FString& F : Files)
		{
			const FString From = FPaths::Combine(BackupDir(), F);
			FSQLiteDatabase Probe;
			if (!Probe.Open(*From, ESQLiteDatabaseOpenMode::ReadOnly)) continue;
			const bool bGood = Probe.PerformQuickIntegrityCheck();
			Probe.Close();
			if (bGood && FM.Copy(*DbPath(), *From) == COPY_OK)
			{
				G.Note = FString::Printf(TEXT("The world file was damaged: restored the backup %s."), *F);
				UE_LOG(LogWildreach, Error, TEXT("STORE world.db was damaged (kept as %s); restored backups/%s"), *FPaths::GetCleanFilename(Bad), *F);
				return true;
			}
		}
		G.Note = TEXT("The world file was damaged and no backup could be read: the world starts empty (the damaged file is kept).");
		UE_LOG(LogWildreach, Error, TEXT("STORE world.db was damaged (kept as %s) and no backup passed its check"), *FPaths::GetCleanFilename(Bad));
		return false;
	}

	bool OpenDb()
	{
		G.Db = MakeUnique<FSQLiteDatabase>();
		if (!G.Db->Open(*DbPath(), ESQLiteDatabaseOpenMode::ReadWriteCreate))
		{
			UE_LOG(LogWildreach, Error, TEXT("STORE can't open %s: %s"), *DbPath(), *G.Db->GetLastError());
			G.Db.Reset();
			return false;
		}
		// Exclusive WAL: fast, crash-safe commits; WAL needs no shared memory this way, and it keeps a second server off
		// the same world folder.
		G.Db->Execute(TEXT("PRAGMA locking_mode=EXCLUSIVE"));
		if (!G.Db->Execute(TEXT("PRAGMA journal_mode=WAL")))
		{
			UE_LOG(LogWildreach, Warning, TEXT("STORE WAL isn't available here (%s): the default journal is used"), *G.Db->GetLastError());
		}
		G.Db->Execute(TEXT("PRAGMA synchronous=NORMAL"));
		G.Db->Execute(TEXT("PRAGMA busy_timeout=5000"));
		return true;
	}
}

void WRStore::Open(UWorld* World)
{
	using namespace WRStoreImpl;
	Close();
	++G.Generation;
	G.World = World;
	if (!G.CleanupHandle.IsValid())
	{
		// Closed the tick after its world is cleaned up, so the world subsystems' last saves (portals, world systems
		// save as they go away) are in it first.
		G.CleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddLambda([](UWorld* Cleaned, bool, bool)
		{
			if (!Cleaned || Cleaned != G.World.Get()) return;
			G.World.Reset();
			const uint32 Gen = G.Generation;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Gen](float)
			{
				if (G.Generation == Gen) WRStore::Close();
				return false;
			}));
		});
	}
	G.Note.Reset();
	G.WorldId = WRWorlds::Current();
	G.Dir = WRWorlds::CurrentDir();
	FString Mode;
	FParse::Value(FCommandLine::Get(), TEXT("-wrstore="), Mode);
	G.bSql = !G.Dir.IsEmpty() && Mode.ToLower() != TEXT("json");
	G.bOpen = !G.Dir.IsEmpty();
	if (!G.bSql)
	{
		if (G.bOpen) UE_LOG(LogWildreach, Log, TEXT("STORE world '%s' keeps JSON files (-wrstore=json)"), *G.WorldId);
		return;
	}
	IFileManager::Get().MakeDirectory(*G.Dir, true);
	const bool bExisted = IFileManager::Get().FileExists(*DbPath());
	if (!OpenDb())
	{
		G.bSql = false;
		G.Note = TEXT("The world store couldn't be opened (is another server running this world?): saves go to JSON files this time.");
		return;
	}
	if (bExisted && !G.Db->PerformQuickIntegrityCheck())
	{
		G.Db->Close();
		G.Db.Reset();
		RestoreNewestBackup();
		if (!OpenDb())
		{
			G.bSql = false;
			return;
		}
	}
	if (!Migrate(*G.Db))
	{
		G.Db->Close();
		G.Db.Reset();
		G.bSql = false;
		G.Note = TEXT("The world store's tables couldn't be made: saves go to JSON files this time.");
		return;
	}
	if (GetMeta(TEXT("world_id")).IsEmpty())
	{
		SetMeta(TEXT("world_id"), G.WorldId);
		SetMeta(TEXT("created_utc"), LexToString(UtcNow()));
	}
	SetMeta(TEXT("game_version"), FApp::GetBuildVersion());
	ImportJson();
	G.LastHourly = UtcNow();
	G.LastDailyDay = FDateTime::UtcNow().GetDayOfYear();
	G.Writer = new FWriter();
	G.Writer->Ms = uint32(FMath::Clamp(WRTune(TEXT("store.flushSeconds"), 10.f), 1.f, 60.f) * 1000.f);
	G.Thread = FRunnableThread::Create(G.Writer, TEXT("WRStoreWriter"), 0, TPri_BelowNormal);
	UE_LOG(LogWildreach, Log, TEXT("STORE world '%s' on SQLite (%s)"), *G.WorldId, *DbPath());
}

void WRStore::Close()
{
	using namespace WRStoreImpl;
	if (G.Thread)
	{
		if (G.bSql)
		{
			FScopeLock Lock(&G.PendingLock);
			G.BackupRequests.Add(TEXT("shutdown"));
		}
		G.Writer->Stop();
		G.Thread->WaitForCompletion();
		delete G.Thread;
		delete G.Writer;
		G.Thread = nullptr;
		G.Writer = nullptr;
	}
	if (G.Db.IsValid())
	{
		FScopeLock Lock(&G.DbLock);
		G.Db->Execute(TEXT("PRAGMA wal_checkpoint(TRUNCATE)"));
		G.Db->Close();
		G.Db.Reset();
	}
	G.bSql = false;
	G.bOpen = false;
	G.Pending.Reset();
	G.InFlight.Reset();
}

bool WRStore::IsSql() { return WRStoreImpl::G.bSql; }
const FString& WRStore::OpenNote() { return WRStoreImpl::G.Note; }

bool WRStore::ReadDoc(const FString& Kind, FString& OutText)
{
	using namespace WRStoreImpl;
	if (!G.bOpen) return false;
	if (!G.bSql) return FFileHelper::LoadFileToString(OutText, *JsonFileFor(EKind::Doc, Kind));
	if (PendingText(EKind::Doc, Kind, OutText)) return true;
	return QueryText(TEXT("SELECT body FROM world_docs WHERE kind = ?1"), Kind, OutText);
}

bool WRStore::HasDoc(const FString& Kind)
{
	FString Unused;
	return ReadDoc(Kind, Unused);
}

void WRStore::WriteDoc(const FString& Kind, const FString& Text)
{
	using namespace WRStoreImpl;
	if (!G.bOpen) return;
	if (!G.bSql)
	{
		FFileHelper::SaveStringToFile(Text, *JsonFileFor(EKind::Doc, Kind));
		return;
	}
	FPending P;
	P.Kind = EKind::Doc; P.Key = Kind; P.Text = Text;
	Queue(MoveTemp(P));
}

bool WRStore::ReadCharacter(const FString& Name, FString& OutText)
{
	using namespace WRStoreImpl;
	if (!G.bOpen || Name.IsEmpty()) return false;
	if (!G.bSql) return FFileHelper::LoadFileToString(OutText, *JsonFileFor(EKind::Character, Name));
	if (PendingText(EKind::Character, Name, OutText)) return true;
	return QueryText(TEXT("SELECT body FROM characters WHERE name = ?1"), Name, OutText);
}

void WRStore::WriteCharacter(const FString& Name, const FString& MainPath, const FString& Text)
{
	using namespace WRStoreImpl;
	if (!G.bOpen || Name.IsEmpty()) return;
	if (!G.bSql)
	{
		IFileManager::Get().MakeDirectory(*FPaths::Combine(G.Dir, TEXT("players")), true);
		FFileHelper::SaveStringToFile(Text, *JsonFileFor(EKind::Character, Name));
		return;
	}
	FPending P;
	P.Kind = EKind::Character; P.Key = Name; P.Text = Text; P.MainPath = MainPath;
	Queue(MoveTemp(P));
}

bool WRStore::ReadAccount(const FString& Name, FAccount& Out)
{
	using namespace WRStoreImpl;
	if (!G.bSql || Name.IsEmpty()) return false;
	{
		FScopeLock Lock(&G.PendingLock);
		const FString K = RowKey(EKind::Account, Name);
		const FPending* P = G.Pending.Find(K);
		if (!P) P = G.InFlight.Find(K);
		if (P)
		{
			Out = P->Account;
			return true;
		}
	}
	FScopeLock Lock(&G.DbLock);
	if (!G.Db.IsValid()) return false;
	FSQLitePreparedStatement S = G.Db->PrepareStatement(TEXT("SELECT name, key_hash, platform, platform_id, first_seen, last_seen, last_address FROM accounts WHERE name = ?1"));
	if (!S.IsValid()) return false;
	S.SetBindingValueByIndex(1, Name);
	if (S.Step() != ESQLitePreparedStatementStepResult::Row) return false;
	S.GetColumnValueByIndex(0, Out.Name);
	S.GetColumnValueByIndex(1, Out.KeyHash);
	S.GetColumnValueByIndex(2, Out.Platform);
	S.GetColumnValueByIndex(3, Out.PlatformId);
	S.GetColumnValueByIndex(4, Out.FirstSeen);
	S.GetColumnValueByIndex(5, Out.LastSeen);
	S.GetColumnValueByIndex(6, Out.LastAddress);
	return true;
}

void WRStore::WriteAccount(const FAccount& Account)
{
	using namespace WRStoreImpl;
	if (!G.bSql || Account.Name.IsEmpty()) return;
	FPending P;
	P.Kind = EKind::Account; P.Key = Account.Name; P.Account = Account;
	Queue(MoveTemp(P));
}

void WRStore::Flush(bool bWait)
{
	using namespace WRStoreImpl;
	if (!G.bSql || !G.Writer) return;
	const uint64 Want = ++G.RequestSeq;
	G.Writer->Wake->Trigger();
	if (!bWait) return;
	// Logout and shutdown: until the writer has committed this (10 s at most; a stuck disk mustn't hang the server).
	const double Until = FPlatformTime::Seconds() + 10.0;
	while (G.CommittedSeq.load() < Want && FPlatformTime::Seconds() < Until)
	{
		FPlatformProcess::Sleep(0.002f);
	}
}

void WRStore::Tick()
{
	using namespace WRStoreImpl;
	if (!G.bSql || !G.Writer) return;
	const int64 Now = UtcNow();
	const int64 Every = int64(FMath::Max(60.f, WRTune(TEXT("store.hourlySeconds"), 3600.f)));
	if (Now - G.LastHourly < Every) return;
	G.LastHourly = Now;
	FScopeLock Lock(&G.PendingLock);
	G.BackupRequests.AddUnique(TEXT("hourly"));
	// The first hourly of a new UTC day is also the day's copy.
	const int32 Day = FDateTime::UtcNow().GetDayOfYear();
	if (Day != G.LastDailyDay)
	{
		G.LastDailyDay = Day;
		G.BackupRequests.AddUnique(TEXT("daily"));
	}
	G.Writer->Wake->Trigger();
}
