# Wildreach build cook: SERVER STORE, IDENTITY, RAIDING, FULL-RELEASE SETTINGS

Lane: build-server-raids. Written 2026-11-14. Builds FULL-GAME 7's server and raid rows in `C:\Dev\Wildreach`.
Design sources followed: FULL-GAME 7; Raven's `M7_server.md` 1 (persistence, identity) and `server-settings.md`
(full-release settings); our `cooks/multiplayer.md` 2.2.2 (identity), 2.4 (persistence), 2.8 (raids);
`cooks/survival-crafting.md` 2.16 (explosives) and 2.18 (raid costs, path raiding, tames);
`cooks/biome-crystal.md` 2.21.4 to 2.21.6 (raid tools, raid bell, path and tame raiding); `cooks/endgame.md` 2.9
(raids "tuned for 8, never capped"). Where a KEPT spec needs a thing the game doesn't have, the fix is written here.

---

## 0. Requirement ledger

Brief + Aaron quotes, split into single requirements. Proof column filled at the end (section 5).

| # | Requirement (source) |
|---|---|
| 1 | Persistence in **SQLite on the server** (scope; M7 1: SQLiteCore, WAL, `world.db` per server) |
| 2 | Tables per M7 1 (accounts, characters, world state...) and **versioned migrations** in `Data/db/migrations/*.sql` applied at server start (M7 1) |
| 3 | **Batched writes on a background thread** every 10 to 30 s, plus on logout and shutdown (M7 1) |
| 4 | **Hourly backups** with rotation (scope; M7 1) |
| 5 | **Migrate from the JSON world files without a wipe** (scope) |
| 6 | **Steam/EOS identity if feasible** (scope; M7 1 "name + generated token in Saved; Steam/EOS later") |
| 7 | **Explosives** (scope; DD Multiplayer; survival-crafting 2.16): fire pot, black powder, powder charge, mining charge |
| 8 | **Raid tools** (scope; crystal 2.21.4): siege ladder, code breaker, raid bell |
| 9 | Raid cost table holds: wood 1 charge, stone 2, metal 9, biome 12; weapons 10% / 2% / 0% (survival-crafting 2.18) |
| 10 | **Path-based raiding** (scope; DD "fire burns wooden walls, necromancers raise the dead inside"; crystal 2.21.6) |
| 11 | **Tames in raids** (scope; DD; survival-crafting 2.18: bite pieces ×0.5, haulers push rams) |
| 12 | **Full-release server settings**: portal break timers, artifact limits, invasion frequency, **raid group sizes** (scope; SS) |
| 13 | Offline protection reduces all raid damage (survival-crafting 2.18; BUILT setting) |
| 14 | Explosives: no friendly fire inside a clan; everyone else in the blast is hit (survival-crafting 2.16) |
| 15 | Data-driven: every number in `Data/*.json` / `tuning.json` (THE BAR, Raven) |
| 16 | All damage to characters through `ApplyHit` (THE BAR) |
| 17 | Mouse AND pad on anything new on screen (THE BAR) |
| 18 | Sounds and effects as table rows (THE BAR) |
| 19 | Server authority, replication, multiplayer edge cases (THE BAR) |
| 20 | No placeholders shown as finished; unfinished stays hidden (THE BAR) |
| 21 | Design with intention: each object's job, connections, how it meets what it touches (Aaron 11-14) |
| 22 | Machines look engineered; moving parts move smoothly, never snap (Aaron 11-14) |
| 23 | Projectiles never home; telegraphs (arc shown) (THE BAR / combat) |
| 24 | Research (games, places, systems) + real-world reference, written with the details used (Aaron 11-13 night) |
| 25 | Plan every detail and build step before building (Aaron 11-13 night) |
| 26 | Spot-check against the plan by reading code (brief) |
| 27 | CHANGELOG "Unreleased" + HANDOFF in the same commit; pull before and after; add files by name (brief) |
| 28 | No release or tag; report new binary sizes (brief) |
| 29 | Compile gate only, fix every error and warning introduced (brief) |
| 30 | Out-of-scope finds go to LATER-COOKS, one line each (brief) |
| 31 | LANDED line at the end (brief) |

---

## 1. Research

### 1.1 Persistence: how live survival servers keep worlds

| Game / system | What it does | What we take |
|---|---|---|
| **Conan Exiles** dedicated server | The whole world lives in one SQLite file, `ConanSandbox/Saved/game.db`; the server keeps rotating copies (`game_backup_1.db`...) and admins fix worlds with any SQLite browser. A crash mid-write loses nothing because SQLite commits atomically | One SQLite file per world, rotating copies beside it. Admin-readable rows (the text bodies are JSON, so a host can read a character in DB Browser) |
| **Rust** | Player data (blueprints, identities) lives in SQLite databases next to the map save; the map save is written to a temp file and swapped in, with `server.savebackupcount` older copies kept | Identity rows in a real table; the swap rule is what SQLite's journal already gives us |
| **Valheim** 0.212+ | World backups are on a short and long timer (`-backupshort` 2 h, `-backuplong` 12 h, `-backups` 4) | Two cadences: hourly (keep 24) and daily (keep 7), plus one on every clean shutdown (keep 5) |
| **ARK** | Timestamped `.ark` copies on every save; corrupt saves are the community's top complaint because the copy can be taken mid-write | Copies are taken with `VACUUM INTO`, which writes a consistent snapshot from inside a read transaction, never a file copy of a live DB |
| **SQLite docs** (WAL, `synchronous=NORMAL`, `VACUUM INTO` since 3.27, `PRAGMA user_version`, `PRAGMA quick_check`) | WAL lets one writer and many readers work at once; NORMAL sync is crash-safe in WAL (only the last commit can be lost on power loss); `user_version` is the standard migration counter | Writer thread + game-thread reader on WAL; migrations keyed on `user_version`; `quick_check` at open, restore from the newest good backup on failure. The engine ships SQLite 3.47.1 (SQLiteCore) |
| **Enshrouded** (characters synced through Steam Cloud rolled back to old saves) | Two sources of truth for one character | The server holds the only copy, as Raven built it |

### 1.2 Identity

| Source | Detail | What we take |
|---|---|---|
| Raven M7 1 | "Platform-less login via a player-chosen name + generated token in the client's Saved dir. Steam/EOS later (EOS is free)" | Built exactly: a 32-byte key in `Saved/Identity/key.bin` |
| Unreal `FUniqueNetIdRepl` | With OnlineSubsystemNull the id is random per launch; with Steam or EOS it's the platform account id and arrives at `PreLogin` | The server binds a platform id to the name the first time one arrives; a later join from the same platform account is accepted even from a new PC |
| **Steam** | Needs a Steamworks app id (the $100 Steam Direct fee); testing on Spacewar (480) is not shippable | Not feasible to ship today (money rule 11-04). Code path ready |
| **EOS** | Free; needs a product registered on the Epic Developer Portal (product id, sandbox, deployment, client id/secret) | Feasible once Aaron registers the product: numbered steps in HANDOFF. The code takes whatever id the online subsystem gives |
| Valheim (characters on the client, editable offline) | The cautionary tale | Character never leaves the server; the key only proves the name |

### 1.3 Raiding: what makes base raids work

| Game / system | What it does | What we take |
|---|---|---|
| **Rust** raid tiers | Each wall tier has a fixed price in each raid tool (timed explosive charge, satchel, beancan, rockets); raiders plan around the "raid cost" of a base; satchels can dud and need a relight; a placed charge sticks to the surface with a visible blinking fuse; explosions hurt anyone nearby, including the raider | A **published cost table** per wall (Rust's clarity); charges **stick to the face you aim at**, show a burning fuse, and hurt anyone in the blast (clanmates spared, KEPT); **no duds** (a dud is a coin flip: our deaths come from play, not dice) |
| **ARK** structures | Tier damage multipliers by source (explosives full, dinos reduced, tools near zero on stone/metal); C4 placed then detonated; Gigas wreck stone | Damage share by **material × source** table; tames bite at ×0.5 and big biters matter |
| **Conan Exiles** | Explosive jars (orbs) thrown or placed; trebuchets; "raid windows" (ruled out by Raven); offline raids felt unfair because thralls don't fight | Fire pot = thrown jar with a visible arc; offline protection (BUILT) cuts everything, explosives included |
| **Rust tool cupboard + code locks** | Code locks can only be beaten by breaking the door; a "code raid" by guessing is a known griefing pain | The code breaker exists, but it's **loud, long (60 s), immobile, and rings the owners' raid bell** at the start, so it's a play, not a backdoor |
| **Sea of Thieves** ship bells | A bell is heard across water and tells everyone a crew is in trouble | The **raid bell**: an in-world bronze bell on the totem, heard 300 m |

### 1.4 Real-world reference

| Source | Detail used |
|---|---|
| **Black powder** | 75 saltpetre : 15 charcoal : 10 sulfur by weight. Saltpetre is the bulk, so it's the bottleneck: recipe 2 saltpetre + 1 charcoal + 1 sulfur → 2 powder |
| **Saltpetre from caves** | Nitre forms as a white, feathery crust on dry cave walls and in cave earth (Mammoth Cave's saltpetre works, 1812; "petre dirt" leached in hoppers) | **Nitre crust** grows on the dry wall of every island cave; scraped with a pick |
| **Clay** | Cave floors and stream beds hold fine sticky clay (cave sediment) | A **clay bank** in every cave, dug by hand or with a pick: the fire pot's jar and the charge's tamping |
| **Byzantine / medieval ceramic grenades** (sphero-conical vessels, 10th to 15th c.) | Thick clay jars filled with incendiary, stoppered, a rag wick lit before the throw | **Fire pot**: a fired clay jar, a rag wick lit at the throw, breaks where it lands and spreads burning fat and sulfur |
| **The petard** (16th to 17th c.) | A bell-shaped bronze or iron pot packed with powder, its mouth bolted to an oak plank (the *madrier*), hung on a gate, a slow match lit; "hoist with his own petard" (Hamlet) | The **powder charge is a petard**: a squat bronze bell on an oak plank, pressed to the wall you aim at, a slow match that burns down in 5 s |
| **Mining charge** | Black powder in a paper or hide cartridge tamped into a drilled hole with clay | **Mining charge**: a hide cartridge tamped with clay into an ore vein: the vein bursts for ×3 ore |
| **Escalade ladders** (medieval siege) | Long ash ladders with iron hooks at the top to bite the parapet, footed in the ditch | **Siege ladder**: 6 m ash rails, rungs pegged through, two iron hooks on top that bite the wall's top edge |
| **Bell acoustics** (campanology) | A tuned bronze bell's partials: hum 0.5, prime 1.0, tierce 1.2 (minor third), quint 1.5, nominal 2.0, superquint 3.0, octave nominal 4.0; the higher ones die fastest | The raid bell's sound is synthesised from those partials (Tools/audio/synth_raid.py), with a strike transient |
| **Tuning-fork resonance** | A struck fork drives a matching body to ring | The **code breaker**: a resonant-crystal fork in a silver frame, held to a keypad; it rings the lock's tumblers loose over 60 s, with a loud beating tone |

### 1.5 Lessons in one line each
1. Every save is one atomic commit; backups are snapshots, never a copy of a live file.
2. A name must be owned by something the player can't type: a key on their PC, later their platform account.
3. Raid costs are published and fixed per wall tier; the tools are visible in the world before they go off.
4. Raiding tools come from the map's economy (sulfur in the Volcanic, saltpetre and clay in caves), so raids cost travel.
5. Defenders hear a raid in the world (a bell), not only in a menu.
6. Paths raid differently (fire burns wood) so a path is a raid role.

---

## 2. Design (every gap solved)

### 2.1 The world store (SQLite)

- **File:** `Saved/Worlds/<id>/world.db` (the world's folder; M7 said `Saved/ServerDB/world.db` from before worlds existed: worlds are folders since 0.6, so the DB lives in its world, and a world folder still copies to a dedicated server whole).
- **Open:** `PRAGMA journal_mode=WAL; synchronous=NORMAL; foreign_keys=ON; busy_timeout=5000`. `PRAGMA quick_check` first; if it fails, the file is renamed `world.db.bad-<utc>` and the newest backup that passes is copied in, with a log Error and a banner to the host: "The world file was damaged: restored the <time> backup".
- **Migrations:** `Data/db/migrations/0001_init.sql` ... applied in order where the file's number > `PRAGMA user_version`, each in a transaction, `user_version` set after each. Shipped with the game (Data is staged).
- **Tables (0001):**
  - `meta(key TEXT PRIMARY KEY, value TEXT)`: `world_id`, `created_utc`, `imported_from_json_utc`, `game_version`.
  - `accounts(name TEXT PRIMARY KEY COLLATE NOCASE, key_hash TEXT, platform TEXT, platform_id TEXT, first_seen INTEGER, last_seen INTEGER, last_address TEXT)`.
  - `characters(name TEXT PRIMARY KEY COLLATE NOCASE, main_path TEXT, saved_utc INTEGER, body TEXT NOT NULL)`: body = the character JSON (paths, inventory, life skills, survival, food, bed, kills, blueprints, quests, artifacts: M7's "inventory blob"; each component already owns its JSON shape, so no column has to change when a system adds a field).
  - `world_docs(kind TEXT PRIMARY KEY, saved_utc INTEGER, body TEXT NOT NULL)`: `world` (pieces, tames, eggs, clock), `clans`, `wildlife`, `portals`, `world_systems`, `raid` (raid logs, below).
  - `backups(file TEXT PRIMARY KEY, kind TEXT, made_utc INTEGER, bytes INTEGER)`: what's on disk, for the rotation.
- **Why documents and not one column per field:** every system in the game (Raven's and every lane since) owns its save shape in its own `ToJson/FromJson`; normalising them would freeze 20 systems' fields into SQL and break each lane's work. Accounts are a real table because identity is checked by query.
- **Writes:** the game thread serialises the JSON (as it does now) and hands `(kind, key, text)` to `FWRStore`; a **writer thread** keeps only the newest text per key and commits all pending rows in **one transaction every 10 s** (`store.flushSeconds`), on logout (that player's row at once), and on shutdown (a synchronous flush that waits for the thread). Players already save every 10 s and the world every 60 s (BUILT timers): unchanged.
- **Reads:** at load, on the game thread, through a second read-only connection (WAL readers never block the writer). A read of a key with a pending write returns the pending text (read-your-writes).
- **Backups:** `VACUUM INTO 'backups/<kind>-<yyyymmdd-hhmm>.db'` on the writer thread: **hourly** while the world runs (`store.hourlyKeep` 24), **daily** at the first hourly after UTC midnight (`dailyKeep` 7), **on clean shutdown** (`shutdownKeep` 5), and **pre-import** (once, kept forever). Oldest beyond each keep count is deleted. Each is about the DB's size (1 to 10 MB).
- **Migration from JSON (no wipe):** at world start, if `world.db` has no `imported_from_json_utc` and any of `world.json`, `clans.json`, `wildlife.json`, `portals.json`, `world_systems.json`, `players/*.json` exist, they're all imported in one transaction; the files then move into `Saved/Worlds/<id>/json-before-sqlite/` (never deleted) with a README line. `info.json` and `settings.json` stay as files: they're the menu's list and the host's editable settings, read before any server runs.
- **`-wrstore=json`:** keeps the old JSON files (Raven's headless test tools read and delete them). Default is SQLite.
- **Edge cases:** two worlds never share a DB (one file per folder); a world folder copied to another PC carries its DB and backups; deleting a world (BUILT) removes the folder with the DB; disk full → the commit fails, the pending rows stay pending and retry next flush, with a log Error each time; a crash between flushes loses at most 10 s.

### 2.2 Identity (M7 1 + multiplayer cook 2.2.2)

- **Client key:** first launch makes 32 random bytes from the OS crypto source (`BCryptGenRandom` on Windows, `/dev/urandom` elsewhere; never `FMath::Rand`) into `Saved/Identity/key.bin`.
- **On join:** the menu adds `?WRKey=<hex SHA-256(key + "wildreach-identity-1")>` to the options (host and joiners). The raw key never leaves the PC.
- **Server, `PreLogin`:** reads `WRName` and `WRKey`. Looks up `accounts`:
  - no row → accepted; `InitNewPlayer` creates it with the key (first claim).
  - row with a key and the join's key matches → accepted.
  - row with a platform id equal to the join's platform id (`UniqueId` from Steam/EOS, type not `NULL`) → accepted, and the key is updated (new PC).
  - row with a key and none or a different one → **refused**: "That name belongs to someone else on this world: pick another name". The join screen already shows the server's refusal text (BUILT network-failure message path).
  - row without a key (imported from JSON, or before identity) → the first key that comes claims it (logged).
- **Stored:** `key_hash` = SHA-256(token + world id), so the DB never holds the token itself; `last_seen`, `last_address`.
- **Lost key:** host deletes the account row (HANDOFF gives the one SQL line; the admin UI is the multiplayer cook's player list, cookpass-multiplayer).
- **Steam/EOS:** see 1.2. Code path live; HANDOFF has Aaron's numbered EOS steps.
- **Headless test bots** (`?WRName=` without a key) still join any name that has no key.

### 2.3 The raid economy (items, sources, recipes)

| Item (id) | Source / recipe | Notes |
|---|---|---|
| Saltpetre (`saltpetre`) | **Nitre crust** node on the dry back wall of every cave (both biomes), pick | White feathery crust over dark rock; yields 2 to 4 |
| Clay (`clay`) | **Clay bank** node on every cave floor by the hazard pool, any tool or hand | A low wet grey-brown mound with scoop marks; yields 2 to 4 |
| Black powder (`black_powder`) | Alchemy table L3: saltpetre 2, charcoal 1, sulfur crystal 1 → 2 | 75/15/10 by weight |
| Fire pot (`fire_pot`) | Alchemy table L2: clay 1, sulfur crystal 1, animal fat 2 → 1 | Thrown (ranged slot, like the bola) |
| Powder charge (`powder_charge`), the petard | Workbench L3: black powder 4, bronze ingot 2, wood 2, fiber 2 → 1 | Pressed onto a piece |
| Mining charge (`mining_charge`) | Workbench L3: black powder 2, clay 1, hide 1 → 1 | Pressed into an ore, stone or sulfur node |
| Siege ladder (`siege_ladder`) | Workbench L3: wood 20, rope 6, iron ingot 1 → 1 | Planted against a wall |
| Code breaker (`code_breaker`) | Workbench L4: steel plate 2, resonant crystal 1, bronze ingot 2 → 1 | KEPT had "silver 2": no silver in the game; bronze is the frame metal (bell bronze rings). Resonant crystal is a Crystal-caves material, so the code breaker is unreachable until that land ships: it stays **hidden** (recipe `liveWith: crystal`) |

All these are recipes in `recipes.json` and items in `items.json` with `note` lines; numbers in `tuning.json raiding`.

### 2.4 Raid damage: one rule for every source

`FWRRaid::DamagePiece(Piece, Amount, Source, By, PathId)` is the only way a player-side source hurts a piece:

1. **Who may:** a player (or their tame, charge, pot, spell) never damages pieces their clan can use (`CanAccess`, BUILT rule); on PvE worlds (`server.pvp` off) player-side sources don't hurt other players' pieces at all ("nothing to raid", as PvP's off), except creature raids (BUILT, unchanged).
2. **Material × source** (`tuning raiding.material.<source>.<material>`):

| Source | wood | stone | metal | biome |
|---|---|---|---|---|
| weapon (BUILT `materialDamage`) | 0.1 | 0.02 | 0 | 0 |
| blast (charge: KEPT absolute numbers, so ×1 here) | 1 | 1 | 1 | 1 |
| firepot | 1 | 0.13 | 0 | 0 |
| spell (path bolts and area spells) | 0.5 | 0.1 | 0.03 | 0.02 |
| tame (bite) | 0.5 | 0.25 | 0.1 | 0.08 |
| fire (burning ground) | 1 | 0 | 0 | 0 |

3. **Path** (`raiding.paths.<path>.<material>`): fire wood ×2 (Raven SPEC), lightning metal ×1.5, spore wood ×1.5, void everything ×1.25, arcane stone ×1.25, the rest ×1. Applied to spells, path-infused weapons (`Weapon.Element`), and fire pots (fire).
4. **Offline protection** (BUILT setting) last, in `TakeBuildDamage` (unchanged).
5. **Raid bell + log:** every hit by a player-side source rings the claim's bell (2.6) and adds to the claim's raid record.

**Charge numbers (KEPT):** blast 4 m: wood 800, stone 600, metal 300, biome 250, Hearth 300; full on the piece it's on, ×0.5 on the others in the 4 m (a wall beside the target cracks, it doesn't fall: so the cost table holds wall by wall). Raid costs check: wood 400 → 1; stone 1,000 → 2; metal 2,500 → 9 (8.3); biome 3,000 → 12. ✔.
**Fire pot (KEPT):** 150 to wood + burning ground 30/s for 10 s (300) on wood pieces in 2 m; stone 20; metal/biome 0; characters 40 + burn 60 buildup. Wood wall 400: 150 + 300 = 450 ≥ 400 → 1 pot if they let it burn; KEPT table says 3 pots: with the burn split over the 2 m patch, a pot that lands at the foot of a wall burns it fully; one on the floor burns the floor. **The table's 3 assumes misses**; kept numbers.
**Characters in a charge blast:** 120 (KEPT) with falloff to 50% at the edge, through `ApplyHit` (`bExplosion`-style: unblockable, `Impact` heavy, origin at the charge), clanmates of the placer skipped, the placer **not** skipped (Rust's lesson: stand back).

### 2.5 The tools in the world (design with intention)

**Fire pot.** *Job:* the cheap early raid tool for wood bases and the anti-crowd throw. *Connects:* sulfur (Volcanic), fat (pigs), clay (caves). *Meets:* thrown from the ranged slot with the bola's throw clip; a rag wick burns on the jar in flight (spark trail); the jar is a code-modelled clay vessel (lathe: round belly, short neck, rolled lip, cloth stopper) in fired-terracotta red-brown. It flies under real gravity, never homes; the throw arc shows while aiming (the ranged aim's arc line, BUILT for throws). It shatters on any hit: shards (`chip_stone` sprites tinted terracotta), a fire burst, and a **burning patch** (AWRHazard `fire`, 2 m, 10 s) that hurts characters (BUILT) and now burns wood pieces it touches.

**Powder charge (petard).** *Job:* the wall-breaker; the cost table's unit. *Connects:* black powder (the cross-map economy), bronze (the petard's bell). *Meets:* Use it from the hotbar while aiming at a piece within 3 m: a server trace finds the face; the petard is placed **flush on that face**, its oak plank (60 × 40 × 6 cm, dark oiled oak with two iron straps) against the wall, the bronze bell (a lathe of a real petard: 28 cm across the mouth, 30 cm long, a squat domed body with a cast ring and a touch-hole boss) bolted mouth-first to the plank's centre, so the bell's base faces you. A slow match (a 40 cm cord, coiled) runs from the touch hole; its glowing end creeps toward the bell over the 5 s fuse (a spark sprite moving along the cord: smooth, not stepped), with a hiss. It's attached to the piece: if the piece dies first, the charge drops (falls under gravity) and still goes off. Anyone can see it; nobody can pull it off (a lit match is lit). Blast: `raid.blast` effect (flash, fireball, dark smoke column, stone and wood chips by the target's material, a shock ring along the ground) and camera shake by distance, the blast sound heard 150 m.

**Mining charge.** *Job:* the miner's use of powder (×3 ore), so powder isn't only for war. *Meets:* used on an ore/stone/sulfur node within 3 m: a hide cartridge tamped into the rock (a short code-modelled cylinder half sunk, clay plug, a fuse cord), 4 s fuse, a smaller blast (dust, rock chips), the node gives its full yield ×3 at once and is used up (it regrows on its BUILT timer). Hurts characters in 2 m for 40 (ApplyHit).

**Siege ladder.** *Job:* lets raiders go **over** a palisade or wall instead of through it (cheaper than charges, but you arrive one at a time into the defenders). *Connects:* placed against any built wall, palisade or gate (not terrain: a ladder against a cliff would be a free climb anywhere). *Meets:* Use it facing a wall within 3 m: the ladder stands with its feet on the ground 1.6 m out from the wall's foot and its top hooks over the wall's top edge (6 m rails; it shortens to the wall's real height up to 6 m and refuses taller: "Too tall for a ladder"). Two ash rails (8 × 5 cm), rungs every 30 cm pegged through, two iron hooks bent over the top. It's an actor with 300 HP that anyone can break with weapons (wood rule), and it lasts 10 min. **Climbing:** walk into it facing it and press forward: you climb at 180 cm/s (`raiding.ladder.climbSpeed`) along the rails (CMC flying mode, set the same on server and client by the overlap: predicted, no snap); back off or jump to drop; at the top you step over onto the wall's top or the walkway behind it (a 120 cm forward hop). Being hit knocks you off (any stagger). Mouse and pad: the same move keys.

**Code breaker.** *Job:* the quiet-ish alternative to blowing a door (a loud, long channel). *Built and hidden* until the Crystal caves ship (resonant crystal). Full behaviour built: use while facing a code-locked door that isn't yours within 2 m → a 60 s channel (you can't move; any hit or moving cancels it); a beating resonant tone heard 40 m; the owners' raid bell rings and their CLAIM ATTACKED word fires at the start; at 60 s the lock opens for you (you're added to the door's `UnlockedFor` and it swings open). Consumed on success; kept if cancelled.

**Raid bell.** *Job:* tells everyone near (defenders and raiders) that the claim is under attack, in the world. *Connects:* hung from the claim totem (BUILT carved post) on an iron bracket bolted round the post below its carved head. *Meets:* a code-modelled bronze bell (lathe from a real bell profile: crown, shoulder, waist, sound bow, lip; 28 cm across, 26 cm tall; bronze, metallic 0.9, roughness 0.35), on a yoke and pin through the bracket's fork; a separate clapper (an iron rod with a ball) hangs inside. When struck, the bell swings as a damped pendulum (±18°, 1.1 s period, decays over 4 s) driven from a replicated strike time, so every machine draws the same swing with no snapping; the clapper lags the bell (its own damped spring) and strikes the sound bow at the swing's ends (the sound plays on the strike, not on a timer). Rings at most once every 2 s. Heard 300 m. **Alert:** every online member of the claim's owner clan (or the solo owner) gets the state word **CLAIM ATTACKED** with the claim's distance and direction and a map ping for 60 s (once per 30 s per claim); offline members get a line at login: "Your claim was attacked 02:14 to 02:40 by Mira and 2 others: 6 pieces broken". The record (last 10 raids per claim, each: start, end, attackers' names, pieces broken, Hearth broken) is saved in the world store (`raid` doc).

### 2.6 Path raiding

- **Bolts** (fire bolt, frost shard, every path's projectile spell): a hit on a build piece (or its station/Hearth child) not yours → `DamagePiece(spell, path)`.
- **Area spells:** nova, meteor, lance hit pieces inside their area once per cast; **fire wall and lava pool** (hazards) burn **wood pieces** they touch every second (fire source, ×2 for fire path: the "fire burns wooden walls" line exactly).
- **Path-infused weapons:** a fire-infused weapon on wood: weapon share × path multiplier.
- **Necromancy raise-through-walls** (KEPT spec): the Necromancy path has no tree yet; the rule is a data row (`raiding.paths.necromancy.raiseThroughWalls` 1, `raiseCostMult` 3) for that lane. LATER-COOKS line.
- **Lightning, spore, void, arcane:** multipliers live in data now; they apply the day those paths' spells exist.

### 2.7 Tames in raids

- **Whistle order "Break that"** (a new wedge on the whistle wheel, slot 3; pad: the same wheel, stick + release): every tame of yours in earshot that is aggressive-capable goes for the **build piece under your crosshair** (server trace from your view, 60 m), if it's not yours/your clan's; they walk to it and bite it every `raid.biteSeconds` at their bite damage × 0.5 (KEPT) through `DamagePiece(tame)`. They stop when it breaks (then each picks the next nearest piece of the same claim within 8 m, so a pack works through a wall line), when you whistle Come/Stay, or when attacked (they fight back: aggressive rule, BUILT). Wedge label "Break that"; dims if nothing breakable is under the crosshair or no tame is in earshot. Response line: "3 tames go for the wood wall".
- The tame order wheel (holding Interact on one tame) gets the same order for that one tame.
- **On PvE worlds** the order refuses ("No raiding on this world") when PvP is off.
- **Haulers push rams:** rams are siege engines (not built: FULL-GAME's siege row, a later cook). LATER-COOKS line.

### 2.8 Full-release server settings

| Setting (SS) | State | Proof / build |
|---|---|---|
| Dungeon portal break timers | BUILT (portals lane) | `server_presets.json` `portalBreak`; `WRPortals.cpp:1001`; settings screen |
| Artifact modifier limits | BUILT (portals lane) | `artifactCap`, `artifactSlots`; `WRArtifacts.cpp:188-243`; `WRUISettings.cpp:1868` |
| Biome invasion frequency | BUILT (world-systems lane) | `invasions`; `WRWorldSystems.cpp:1824`; settings screen |
| **Raid group sizes** | **Build here** | `raidGroupMax` (0 = no cap, 2 to 16) and `raidGroupTuned` (the group the raid's numbers are balanced for, 4 to 12, default 8: endgame 2.9.1 "tuned for 8, never capped", DR #22). Read by `FWRSocialRules::RaidGroupMax/Tuned`, used by the war-halls when the endgame lane builds them. **The two settings rows stay hidden until a war-hall exists** (`raiding.warHallsLive` 0 in tuning; the endgame lane flips it), so nobody sees a setting that does nothing |

### 2.9 Screens, prompts, devices

- Everything new is a world action on existing keys: Use (hotbar) places charges/ladders/code breakers; the ranged throw throws pots; the whistle wheel has one new wedge (mouse and pad already drive the wheel); the ladder climbs on the move keys. Prompts go through `WRInput::ActionKey` so pads see their buttons.
- No new screen. The settings screen gets the two raid-group rows (hidden until live, 2.8).

### 2.10 Sound and effects (table rows)

| Event | Sound | Effect |
|---|---|---|
| `raid.bell` | Synth bronze bell (campanology partials, 3 variants), radius 30,000 | none (the bell moves) |
| `raid.fuse` | Synth slow-match hiss + crackle, 5 s, radius 1,500 | `raid.fuse` sparks at the match's end |
| `raid.blast` | Synth blast: sub thump + noise burst + debris rattle tail, radius 15,000, duck 4 dB | `raid.blast` (flash, fireball, smoke column, chips, ground ring) |
| `raid.blast_small` | the blast pitched up, radius 6,000 | `raid.blast_small` (dust + chips) |
| `raid.pot_break` | clay shatter (existing stone chip transients pitched up) + fire whoomp (existing fire impact) | `raid.pot` (terracotta shards + fire burst) |
| `raid.codebreaker` | synth beating resonant tone loop, radius 4,000 | glints at the keypad |
| `raid.ladder` | existing wood placement / creak | none |

Files: `Data/Audio/raid/*.ogg`, made by `Tools/audio/synth_raid.py` (our own synthesis; CREDITS line).

### 2.11 Multiplayer and edge cases

| Case | Handling |
|---|---|
| Charge placed, placer dies or logs out | The charge is its own actor; it goes off |
| Charge on a piece that breaks before the fuse ends | Detaches, falls, blasts where it lands |
| Two charges on one wall | Both go off; the second hits what's left |
| Charge on your own/clan piece | Refused ("That's yours") |
| PvE world | Charges, pots and tames refuse other players' pieces ("No raiding on this world"); mining charges and pots on creatures still work |
| Raider in a safe zone | Pieces can't exist there (BUILT no-building) |
| Offline protection | Applies to every raid source (TakeBuildDamage) |
| Ladder's wall breaks | The ladder falls (destroyed) and anyone on it drops |
| Ladder against something not a wall | Refused ("Lean it on a wall") |
| Code breaker on a door with no lock / your own | Refused |
| Bell spam | Rings at most every 2 s; alert to members at most every 30 s per claim |
| Store write fails (disk full, locked) | Pending rows kept and retried each flush; log Error |
| Two servers on one world folder | The second server's writes wait `busy_timeout` then fail and stay pending with a log Error naming the cause (one server per world folder; HANDOFF says so) |
| Name taken by a key | Refused at PreLogin with the plain reason |

---

## 3. Plan: build steps

1. `Wildreach.uproject`: enable `SQLiteCore`. `Wildreach.Build.cs`: `SQLiteCore` dependency.
2. `Data/db/migrations/0001_init.sql` (tables in 2.1).
3. New `WRStore.h/.cpp`: `FWRStore` (Open/Close per world, migrations, quick_check + restore, writer thread with pending map + 10 s commit, Read/Write/Flush/Remove, ImportJson, Backup(kind) with rotation, hourly timer driven by the game mode); `WRIdentity` (client key + token; server account check/bind).
4. Route saves: `AWRGameMode` SaveWorld/LoadWorld/SavePlayer/LoadPlayer, `FWRClans` Load/Save, `WRWildlife` Save/Load, `WRPortals` save/load, `WRWorldSystems` save/load → `FWRStore::Get().Write/Read(kind, key)`; `-wrstore=json` keeps files.
5. `AWRGameMode`: `PreLogin` (identity), `InitNewPlayer` (bind account), backups timer, flush on Logout/EndPlay; store open in `InitGame` after `SetCurrent`.
6. `WRMainMenu::Options()`: `?WRKey=`.
7. New `WRRaid.h/.cpp`: `FWRRaid::DamagePiece`, `SplashPieces`, `RingBell`/alerts, raid log in the store; `AWRRaidCharge` (petard + mining charge actor, code-modelled meshes, fuse, blast); `AWRSiegeLadder`; code breaker channel; helpers to place them (`FWRRaid::UseItem`).
8. Hook damage: melee (`WRAbilities.cpp` raiding block → DamagePiece weapon), projectile OnHit (piece hits for spells, fire pot burst), area spells (nova/meteor/lance), `AWRHazard::Tick` (fire/lava burn wood pieces), tames (whistle "Break that" + `TickTame` piece bite).
9. Items/recipes/data: `items.json`, `recipes.json`, `tuning.json raiding`, `server_presets.json` (raid group keys), `sounds.json`, `fx.json`; cave nodes (saltpetre, clay) in `AWRCave::SpawnContents` + their looks in `WRResourceLook.cpp`.
10. Raid bell component on `AWRClaimTotem` (lathe bell + bracket + clapper, pendulum from replicated strike time), CLAIM ATTACKED word + map ping (client RPC), login summary.
11. Ladder climbing in `UWRCharacterMovementComponent`/character (overlap → flying along rails).
12. Settings: `WRWorlds` raid group numbers; `WRUISettings` rows hidden until `raiding.warHallsLive`.
13. Sounds: `Tools/audio/synth_raid.py` → `Data/Audio/raid/*.ogg`.
14. CHANGELOG Unreleased, HANDOFF (store, identity, EOS steps, raiding), CREDITS line.
15. Compile, fix, spot-check vs this doc, commit, push, LANDED.

---

## 4. Spot-check notes (read against this plan and THE BAR after building)

| Found | Fixed |
|---|---|
| The store closed itself the tick after the old world's cleanup, which could close a world opened in between (menu → host) | A generation counter: the queued close only closes the store it was queued for (`WRStore.cpp` Open / cleanup lambda) |
| A charge that fell off a broken piece swept a root with no collision, so it never landed | Line-traced fall, lands on the face below and lies on its plank (`AWRRaidCharge::Tick`) |
| A petard on a floor or ceiling had a degenerate facing (normal parallel to up) | `MakeFromX(normal)` for every face |
| The "while you were away" lines were three notices in a row (only the last shows) and came during the loading cover | One card, 10 s after login (`raiding.awayCardSeconds`) |
| No throw arc existed for thrown items (plan 2.5 assumed it did) | Built: `FWRRaiding::TickThrowArc` draws the server's own throw path as dots + landing ring while aiming a fire pot |
| Unity builds: file-scope `using namespace` in WRRaidLook.cpp would leak into the next file of its blob | Moved inside `namespace WRRaidLook`; full unity build passes |
| Plan 2.3 said code breaker frame "silver"; no silver exists | Bronze (bell metal), recipe hidden with `biome: crystal` until resonant crystal is reachable |
| Two perks waited for "explosives" | Made live: Siege-proof (`BlastResist` saved per wall), Silent charges (`raid.blast_quiet`, 5,000 cm) |
| Not built (and why) | Siege engines + haulers pushing rams (siege lane: biome-crystal 2.21.5), Necromancy raise-through-walls (no Necromancy tree; numbers waiting in `raiding.paths.necromancy`), ladder climb clip (no clip in the hero set), Backups screen and admin "release name" (cookpass-multiplayer: multiplayer cook 2.4 / 2.15), EOS enable (needs Aaron's product ids: HANDOFF steps) |

## 5. Ledger proof

| # | Requirement | Done | Where |
|---|---|---|---|
| 1 | SQLite on the server | Yes | `WRStore.cpp:514` Open (SQLiteCore, WAL exclusive), `Wildreach.uproject` SQLiteCore, `Wildreach.Build.cs` |
| 2 | Tables + versioned migrations | Yes | `Data/db/migrations/0001_init.sql`; `WRStore.cpp:106` Migrate (user_version) |
| 3 | Batched background writes, logout, shutdown | Yes | `WRStore.cpp` FWriter (flushSeconds 10), `WRGameMode.cpp` Logout/EndPlay `WRStore::Flush`, `Wildreach.cpp` ShutdownModule Close |
| 4 | Hourly backups with rotation | Yes | `WRStore.cpp:731` Tick (hourly/daily), `:235` MakeBackup (VACUUM INTO, keep counts), shutdown backup in Close |
| 5 | Migrate JSON without a wipe | Yes | `WRStore.cpp:385` ImportJson (one transaction, pre-import snapshot, files kept in json-before-sqlite/) |
| 6 | Steam/EOS identity if feasible | Partly (code ready; EOS needs Aaron's product ids, Steam a paid app id) | `WRIdentity.cpp:120` Check (platform id), `WRGameMode.cpp:95` PreLogin, `WRMainMenu.cpp` `?WRKey=`; HANDOFF EOS steps |
| 7 | Explosives | Yes | items/recipes (fire_pot, black_powder, powder_charge, mining_charge); `WRRaiding.cpp:928` Explode, `:656` FirePotBurst |
| 8 | Raid tools: ladder, code breaker, bell | Yes (code breaker hidden until resonant crystal) | `AWRSiegeLadder` + `WRCharacterMovementComponent.cpp:135` PhysCustom; code breaker in `UseItem`/`Step`; `WRRaiding.cpp:267` RingBell, `:1121` bell swing, `WRBuilding.cpp:1462` HangBell |
| 9 | Raid cost table holds | Yes | `tuning.json raiding.charge` (800/600/300/250 vs walls 400/1000/2500/3000) |
| 10 | Path-based raiding | Yes for Fire/Frost (the built paths); data rows for lightning/spore/void/arcane; necro waits for its tree | `WRRaiding.cpp:361` DamagePiece / Scale; `WRAbilities.cpp:2145` burning ground, `:2179` nova; `WRProjectile.cpp` bolts on pieces |
| 11 | Tames in raids | Yes (rams: not built, no siege engines) | `WRCreature.cpp:3054` TickSiege; `WRRaiding.cpp:682` OrderTamesToBreak; `WRPlayerUI.cpp:951` wheel |
| 12 | Full-release settings | Yes | portalBreak `WRPortals.cpp:1001`, artifactCap `WRArtifacts.cpp:188`, invasions `WRWorldSystems.cpp:1824`; raidGroupMax/Tuned `server_presets.json`, `WRRaiding.cpp:473`, `WRUISettings.cpp:1871` |
| 13 | Offline protection on all raid damage | Yes | DamagePiece → `TakeBuildDamage` (unchanged protection) |
| 14 | No friendly fire in clan; others hit | Yes | `WRRaiding.cpp:196` BlastHits |
| 15 | Data-driven | Yes | `tuning.json raiding`, `store`; items/recipes/sounds/fx rows |
| 16 | Character damage through ApplyHit | Yes | `Burst` → `UWRCombatLibrary::ApplyHit` |
| 17 | Mouse and pad | Yes | world actions on existing keys; whistle wheel (pad-driven); settings rows via the screen kit |
| 18 | Sounds/fx as rows | Yes | `sounds.json raid.*`, `fx.json raid.*`; `Tools/audio/synth_raid.py` |
| 19 | Server authority, replication, edge cases | Yes | all decisions on the server; charge/ladder/bell replicated; edge cases 2.11 |
| 20 | Unfinished stays hidden | Yes | code breaker recipe `biome: crystal`; raid group rows behind `warHallsLive` |
| 21 | Design with intention | Yes | 2.5 per object; bell strapped to the post at its traced radius, petard on its plank on the face, ladder hooks over the wall top |
| 22 | Engineered, smooth motion | Yes | bell pendulum + lagging clapper from a replicated strike time; match glow walks the cord; ladder eased onto its lane |
| 23 | Never homes; telegraphs | Yes | fire pot under gravity; arc drawn while aiming; 5 s visible fuse |
| 24 | Research | Yes | section 1 |
| 25 | Plan before build | Yes | sections 2, 3 |
| 26 | Spot-check | Yes | section 4 |
| 27 | CHANGELOG + HANDOFF, git rules | Yes | commit (CHANGELOG "Unreleased: the world store...", HANDOFF Where the game stands) |
| 28 | No release; binary sizes | Yes | 8 OGG 235 KB + 8 PNG 60 KB |
| 29 | Compile gate | Yes | editor build (adaptive + full unity) succeeded, no warnings |
| 30 | LATER-COOKS | Yes | lines appended |
| 31 | LANDED | Yes | appended |
