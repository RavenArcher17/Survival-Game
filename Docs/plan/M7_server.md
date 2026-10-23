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

## 2. Server settings (from the Server settings tab)
`Config/DefaultWildreachServer.ini` → `UWRServerSettings` (UDeveloperSettings), read at startup and replicated to clients where relevant.
Official servers run one of three **locked presets**; community servers can change nearly everything.

| Setting | Range | PvP standard | PvE | Casual co-op |
|---|---|---|---|---|
| PvP | on/off | On | Off | Off |
| Death penalty | Gentle / Souls-like / Drop inventory / Full loot | Souls-like | Souls-like | Gentle |
| Offline base protection | 0–100% less damage | 75% | — | — |
| PvP inside boss arenas | on/off | On | — | — |
| Bounties and infamy | on/off | On | Off | Off |
| Clan size limit | 2–50 or none | 8 | 12 | none |
| XP and skill gain | 0.5–10× | 1× | 1× | 2× |
| Gathering and drops | 0.5–10× | 1× | 1× | 2× |
| Taming and breeding speed | 0.5–10× | 1× | 1× | 3× |
| Crop and crafting timers | 0.5–10× faster | 1× | 1× | 2× |
| Boss respawn time | 1–24 h | 4 h | 4 h | 2 h |
| Base upkeep cost | 0.5–3× | 1× | 1× | 0.5× |
| Creature raids on bases | off/rare/normal/frequent | normal | normal | rare |

**Fixed on every server** (not exposed in the ini): the 10% boss-loot damage rule, the 150-point / 100-per-path cap,
artifact weird rules applying only to the wearer.

**Playtest preset (internal, not in the design doc):** at 1× a path takes ~80 h and a life skill ~50 h, so playtesters
would never reach the higher nodes. `Playtest` = Casual co-op values plus XP and skill gain **10×** (the doc's max),
taming 10×, boss respawn 1 h. Used for every internal playtest from M2 on; debug commands
(`WR.GrantPath`, `WR.GrantSkill`) cover anything faster.

Implementation: `EWRServerPreset` {PvPStandard, PvE, CasualCoop, Playtest, Custom}; a preset fills defaults and locks them
unless the preset is `Custom`. Every gameplay system reads multipliers from `UWRServerSettings` and never hard-codes them.
Full release adds: portal break timers, artifact modifier limits, invasion frequency, raid group sizes.

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
