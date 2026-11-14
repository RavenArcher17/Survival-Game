-- Wildreach world store, schema 1 (Docs/plan/M7_server.md 1; cooks/build-server-raids.md 2.1).
-- One file per world: Saved/Worlds/<id>/world.db. Applied by WRStore when PRAGMA user_version < 1.
-- Each system keeps its own save shape as JSON in a body column, so a system that adds a field needs no migration;
-- names and keys that the server looks things up by are real columns.

CREATE TABLE IF NOT EXISTS meta (
	key   TEXT PRIMARY KEY,
	value TEXT
);

-- Who owns each character name on this world (WRIdentity).
CREATE TABLE IF NOT EXISTS accounts (
	name         TEXT PRIMARY KEY COLLATE NOCASE,
	key_hash     TEXT,              -- SHA-256 of the client's identity token + the world id ("" = unclaimed)
	platform     TEXT,              -- online subsystem type once Steam/EOS is on ("" = none)
	platform_id  TEXT,              -- that subsystem's account id
	first_seen   INTEGER NOT NULL DEFAULT 0,   -- unix seconds, UTC
	last_seen    INTEGER NOT NULL DEFAULT 0,
	last_address TEXT
);

-- One row per character: paths, inventory, life skills, survival, food, bed, kills, blueprints, quests, artifacts.
CREATE TABLE IF NOT EXISTS characters (
	name      TEXT PRIMARY KEY COLLATE NOCASE,
	main_path TEXT,
	saved_utc INTEGER NOT NULL DEFAULT 0,
	body      TEXT NOT NULL
);

-- One row per world system: world (pieces, tames, eggs, clock), clans, wildlife, portals, world_systems, raid.
CREATE TABLE IF NOT EXISTS world_docs (
	kind      TEXT PRIMARY KEY,
	saved_utc INTEGER NOT NULL DEFAULT 0,
	body      TEXT NOT NULL
);

-- The snapshot files in backups/, for their rotation (hourly, daily, shutdown, import).
CREATE TABLE IF NOT EXISTS backups (
	file      TEXT PRIMARY KEY,
	kind      TEXT NOT NULL,
	made_utc  INTEGER NOT NULL,
	bytes     INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS backups_by_kind ON backups(kind, made_utc);
