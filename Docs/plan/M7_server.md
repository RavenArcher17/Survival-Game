# M7: Server & ship to friends

**Goal:** a persistent dedicated server friends can join from their own PCs, running the whole First Playable.

**Gate (the First Playable gate):** a group of friends plays a full evening on the server and wants to come back.

Depends on: everything. Persistence and the bot harness should start earlier (they're listed here for completeness).
**Server settings are still being designed:** section 2 is the framework, and its values wait for Raven.

## 1. Persistence (no wipes)
| Piece | Detail |
|---|---|
| Store | SQLite file per server (`Saved/ServerDB/world.db`) via the engine's `SQLiteCore`; WAL mode |
| Tables | accounts, characters (location, vitals, inventory blob), path_progress, life_skills, tames, claims, building_pieces, containers, hearths, world_state (boss timers, ore respawns, time of day) |
| Writes | Dirty-flag + batched writes on a background thread every 10–30 s, plus on logout and shutdown |
| Migrations | Versioned SQL migrations in `Data/db/migrations/*.sql`, applied at server start |
| Backups | Hourly copy of the DB to `Saved/ServerDB/backups/` with rotation |
| Identity | First Playable: platform-less login via a player-chosen name + generated token in the client's Saved dir. Steam/EOS later (EOS is free). |

## 2. Server settings (framework only, values TBD by Raven)
`Config/DefaultWildreachServer.ini` → `UWRServerSettings` (UDeveloperSettings), read at startup and replicated to clients where relevant:
- PvP on/off (PvE-only option), safe-zone rules
- Death penalty mode (Gentle / Souls-like / Drop inventory / Full loot)
- Offline raid protection %, upkeep rates, boss respawn timers
- Max players, clan size limit, tame limits
- XP/gather/taming rate multipliers

## 3. Social
- Small clans: create/invite/kick, shared claim and tame access, clan chat. Ranks and alliances are Full release.
- Text chat (global, local by distance, clan).

## 4. Packaging & hosting ($0)
| Step | Detail |
|---|---|
| Source engine | Link the Epic account to GitHub (free) → clone UE 5.8 source, build it once (~2–3 h, ~200 GB). Needed only for packaged **dedicated server** builds. Keep developing on the launcher engine. |
| Builds | `Tools/package.ps1`: client (Win64 Shipping) + server (Win64 and Linux Server targets) via BuildCookRun |
| Hosting v1 | Raven's PC runs the server; friends connect by IP (UDP 7777 port-forward) or through a free tunnel |
| Hosting v2 | Free-tier cloud VM if one fits a UE Linux server's RAM needs (evaluate when there's a real build to measure) |
| Distribution | Client zip shared directly or via itch.io (free) as a private build |

## 5. Load & soak testing
- Bot client: `-nullrhi` client with `AWRBotController` that wanders, fights, gathers, builds; launched N at a time by `Tools/bots.ps1`.
- Targets: 20 bots for every milestone; 100 bots before inviting friends. Watch server frame time, bandwidth per client, DB write latency.
- 8-hour soak: memory growth, DB size, no crash.

## 6. Anti-cheat basics
Server authority everywhere already covers most of it. Add: RPC rate limits, movement speed sanity checks, and validation on every inventory/build/point RPC.
