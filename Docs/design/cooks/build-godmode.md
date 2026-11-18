# build-godmode: the testers' god mode (Aaron, Raven, Colin only)

Lane build-godmode, 2026-11-15, desktop. Aaron 11-15: "For my account [Aaron's account] [kept out of the repo], I
need to have a god mode in Wildreach so I can test everything. This is something that should only be available to Aaron,
Raven and Colin and just for testing." and "It will be hosted through freedom 35, so once that happens you'll know I'm
signed in."

## 0. Requirement ledger

Every row gets its proof (file:line + commit) in section 5 when built.

| # | Requirement (from the brief and Aaron's quotes) |
|---|---|
| R1 | A god mode in Wildreach for testing everything (Aaron 11-15). |
| R2 | Only Aaron, Raven and Colin, decided by their signed-in Freedom35 Studios accounts. |
| R3 | Never decided by a name typed in the game, a PC, a config file or a command-line flag a player could copy. |
| R4 | Aaron's account is on the allow list. |
| R5 | Raven's account is on the allow list. |
| R6 | Colin's account found on the site and allowed (else: one row to add + a question). |
| R7 | Every other player: no menu entry. |
| R8 | ... no key. |
| R9 | ... no console command. |
| R10 | ... refused by the server. |
| R11 | No separate tester sign-in, device code or extra button: identity from the Freedom35 hosting. |
| R12 | Reuse the platform's PC sign-in (pc links, pclinks.js, sdk.js), never a parallel system. |
| R13 | The site says whether the account is a tester, from a server-side allow list. |
| R14 | It hands the game a signed proof the game / server can verify. |
| R15 | The proof is bound to the account. |
| R16 | ... bound to this PC's WRIdentity token. |
| R17 | ... with an expiry. |
| R18 | If the hosting isn't wired yet, the game reads the identity exactly as the platform will hand it over, so god mode switches on by itself. |
| R19 | Site side in freedom35-studios, its patterns, deployed with its tool, no token printed. |
| R20 | Multiplayer: the server / host decides; the client sends its proof at join; the server verifies; a replicated tester flag. |
| R21 | Every god action is a server RPC that re-checks the flag; a non-tester is refused and logged. |
| R22 | Research the debug menus of the genre (Valheim, ARK, Elden Ring...) and take what helps test every system. |
| R23 | Invulnerable. |
| R24 | Infinite stamina. |
| R25 | Infinite mana (every path's bar). |
| R26 | Infinite hunger and thirst. |
| R27 | One-hit kill toggle. |
| R28 | Fly, with a speed. |
| R29 | Noclip. |
| R30 | Teleport to any biome. |
| R31 | ... any POI. |
| R32 | ... any shrine. |
| R33 | ... any boss arena. |
| R34 | The teleport list comes from the game's data, not hand-typed. |
| R35 | Spawn any creature. |
| R36 | Spawn any boss. |
| R37 | Spawn any item / resource in any amount. |
| R38 | ... any recipe. |
| R39 | Unlock every path. |
| R40 | ... every ability. |
| R41 | ... every recipe. |
| R42 | ... every building piece. |
| R43 | ... every armour set. |
| R44 | Set level / XP. |
| R45 | Set the time of day. |
| R46 | Set the weather. |
| R47 | Set the season. |
| R48 | Trigger invasions and events. |
| R49 | Complete or reset any quest step. |
| R50 | Revive. |
| R51 | Kill all nearby. |
| R52 | Freeze AI. |
| R53 | Show hitboxes / aggro if cheap. |
| R54 | A clean god-mode panel, keyboard + mouse and pad. |
| R55 | A hotkey to open it. |
| R56 | A small "GOD" badge on screen while any of it is on. |
| R57 | God-mode use never touches what other players compete on or keep: the character / world is marked tested. |
| R58 | No achievements from a god-mode session. |
| R59 | No leaderboards from a god-mode session. |
| R60 | Worlds on public servers stay clean. |
| R61 | F8 reports from a god-mode session say so in their context. |
| R62 | The allow list lives server-side only; no email in code, comments, commits or anything public. |
| R63 | No secret committed or logged. |
| R64 | Full cook: research + plan written before building; ledger proven. |
| R65 | Compile gate clean (no new errors or warnings). |
| R66 | CHANGELOG (Unreleased) and HANDOFF in the same commit. |
| R67 | Land in pieces: pull --rebase, commit by name, push master after each part. |
| R68 | LANDED line; questions to QUESTIONS-FOR-TOMORROW.md. |
| R69 | Don't break the site's playable gate. |
| R70 | Numbers in Data/tuning.json (data-driven). |

## 1. Research

### 1.1 What the genre's debug and admin modes do (and what to take)

- **Valheim devcommands** (F5 console, `devcommands` then commands; `debugmode` adds hotkeys). `god` (no damage taken,
  stamina never drains), `ghost` (creatures ignore you), `fly` / `Z` in debug mode (Space up, Ctrl down, looking where
  you go), `freefly` (detached camera), `killall` (everything near), `spawn <prefab> <amount> <level>`, `tod <0..1>`,
  `env <name>` (force a weather), `event <name>` (start a raid event), `raiseskill <skill> <n>`, `goto x z`,
  `location <name>` / `exploremap`, `nocost` (build and craft without materials), `tame`, `removedrops`. Since the
  2021 patches **Steam achievements are switched off while devcommands are on**. What works: the very short list of
  toggles that matter (god, ghost, fly, killall) and that every system has a jump-start (`raiseskill`, `event`, `env`).
  Take: invulnerable + endless stamina as two separate toggles (sometimes you want to test running out), "unseen"
  (ghost), fly with rise / sink on Jump / Dive, kill-all with a radius, spawn with an amount, event / weather triggers,
  the achievements switch-off.
- **ARK: Survival Evolved admin cheats** (`enablecheats <password>`, server-side; later the in-game **Admin Manager**
  panel `ShowMyAdminManager` with Players / Tribes / Cheats tabs). `god`, `infinitestats` (food, water, stamina,
  torpor), `fly`, `ghost` (noclip; `walk` ends both), `TeleportToPlayer`, `TPCoords`, `GiveItem <bp> <qty> <quality>`,
  `Summon` / `GMSummon` (tamed, level), `GiveEngrams` (every recipe learned), `AddExperience`, `SetTimeOfDay hh:mm`,
  `DestroyWildDinos`, `ForceTame`, `EnemyInvisible` (AI ignores you), `PlayersOnly` (**freezes every creature in
  place**: the standard way to set up a fight or a screenshot), `ToggleHud`. Every admin command is written to the
  server log (with Steam id), which official-server staff audit. Take: a tabbed panel instead of a console, `GMSummon`'s
  "spawn tamed", `GiveEngrams`' learn-everything, PlayersOnly's freeze, the per-command server log line, and ARK's
  biggest lesson: **an admin password is copyable** (leaked passwords are why ARK admin abuse was common), so who is a
  tester must come from an account, not a secret typed into the game.
- **Elden Ring / FromSoftware debug tooling** (the developer DebugMenu the community rebuilt in DSR / ER debug tools and
  CE tables testers use): "No Dead" (you can't die), "No Damage", "No Stamina Consumption", "No FP Consumption",
  "No Goods Consumption", "All No Update AI" / "Disable AI" (every enemy frozen in its pose), "Warp to Site of Grace"
  (a list of every grace from the game's own table, grouped by region), "Event flag" editing (quest steps on / off),
  hitbox and target display (capsules, attack spheres, the lock-on target line). Take: warp from the game's own list of
  places grouped by land, quest-step stepping, capsule + aggro-line display.
- **Minecraft (Bedrock)**: turning cheats on asks first and then **disables achievements for that world for good**; the
  world keeps the mark even if cheats go off again. Java's "Allow cheats" and spectator mode (noclip flight). Take: the
  permanent mark on the world, and say so up front, not afterwards.
- **Terraria Journey mode**: a power menu at the left (godmode, time freeze and set, weather, spawn rate, item
  duplication/research); a Journey character is marked as such (its own colour in the character list) and cannot enter
  normal worlds, and normal characters cannot enter Journey worlds. Take: the character itself carries the mark, shown
  where it's seen.
- **Satisfactory 1.0 Advanced Game Settings** (No Build Cost, God Mode, Flight Mode, No Power, unlock all): enabling them
  **flags the save permanently** and disables achievements for it, with a clear warning before. Take: a warning banner
  in the panel before the first use, the permanent flag.
- **7 Days to Die**: debug mode (F1 `dm`), creative menu, god (Q) and fly (H) keys; who may use them is
  `serveradmin.xml` on the server, a list of **platform account ids** with permission levels. Take: the allow list
  lives on the server and names accounts, not machines.
- **Rust**: `ownerid` / `moderatorid <steamid>` server-side; admins show a tag; `god`, `noclip`, `debugcamera`,
  `ent kill`. Take: others can see who has powers on (a tag on the name), so footage and play aren't confused.

### 1.2 How platforms prove "who is signed in" to a game server (identity research)

- **Steam session tickets**: the client calls `GetAuthSessionTicket`, sends it to the game server, which calls
  `BeginAuthSession` (offline-verifiable signature from Steam, bound to that session) and gets the SteamID. The ticket
  is per session: replaying one elsewhere fails.
- **Epic Online Services Connect ID tokens**: a JWT signed by Epic; game servers verify it **offline** against Epic's
  published public keys (JWKS), checking `exp`, audience and the deployment. No shared secret on the game server.
- **Xbox Live XSTS tokens** carry a relying-party audience so a token minted for one service can't be used on another.
- Lessons: (1) the game server must not hold a signing secret (a listen server is a player's PC), so use an
  **asymmetric signature** checked against a public key compiled into the game; (2) bind the proof to the session with a
  **server-chosen nonce** so a proof captured by one host can't be replayed on another; (3) short expiry; (4) an
  audience field so the proof can't be confused with any other token the site mints.

### 1.3 What Freedom35 already has (read in freedom35-studios)

- `functions/api/pc/start|approve|token.js` + `_lib/pclinks.js`: RFC 8628 device link. `F35Update.exe` (the updater
  that launches every downloaded game, `cli/F35Update.cs`) links the PC once and keeps the token per Windows user in
  `%LOCALAPPDATA%\Freedom35\pc-link.bin`, DPAPI-encrypted (`ProtectedData.Protect`, CurrentUser).
- `requirePcLink(ctx)`: the PC calling and its account, from the `Authorization: Bearer f35pc_...` header.
- `api/pc/game/[slug]/[action].js` `token` action + `_lib/sdk.js mintGameToken`: You're the Cupcake reads the same
  pc-link.bin itself and asks the site for a 10-minute signed game token (README "You're the Cupcake: game sign-in").
  That's exactly "identity from the hosting": the game reads what the updater stored, no new sign-in.
- `playerKey(secret, userId, scope)`: an opaque per-scope account key, so no account id or email reaches the game.
- `requireAdmin` (x-admin-token header) for admin tools (`tools/reports.mjs` pattern).
- Users table: Aaron (admin), Raven (creator) and **Colin: two accounts named Colin Carson** (a creator account from
  2026-10-04 and a player account from 2026-11-09). Wildreach is not yet a game row on the site.

### 1.4 What Wildreach already has (read in the repo)

- `WRIdentity::Token()`: 32 random bytes per PC in `Saved/Identity/key.bin`; joins send `?WRKey=<token>`; the server's
  `AWRGameMode::PreLogin/InitNewPlayer` read it (`WRGameMode.cpp:110/138`).
- Dev commands: ~45 `Exec` helpers send `ServerCommand("...")`, run by `RunServerCommand`
  (`WRPlayerController.cpp:1159`) and `<system> dev ...` (`WRWorldSystems::Dev`, `WRPortals`, `WRDeeds`,
  `WRArtifacts`, `WRSwampSystems`). **In packaged builds (Development config) they are open to every player** (only
  Shipping filters them, `WRPlayerController.cpp:1139`). Already-built jump-starts to reuse: `tp`, `give`, `spawn`,
  `hour`, `weather <biome> <kind> <min>` (`UWRWeatherSubsystem::Force`), `world dev event|invasion|quest|found`,
  `hearthlevel`, `givepoints` / `autospend`, `setmain` / `setsub`, `skill`, `tamedev now` (`CompleteTame`), `hold`.
- Damage: everything through `UWRCombatLibrary::ApplyHit` (`WRCombatLibrary.cpp:417`); the health write is at
  `:945`; death is `AWRCharacterBase::Die` (`WRCharacterBase.cpp:227`).
- Places: `FWRPoi::All` (camps, caves, passes, landmarks, bosses), `AWRShrine` / `AWRVillage` actors,
  `world.json biomes.<id>.centre`, `AWRPortal`, the lands' camps; the map's `Gather` (`WRUIMap.cpp:342`).
- Creatures: `FWRCreatureRegistry` (207 rows, 62 bosses), `AWRCreature::SpawnCreature` (bosses too); AI brains in
  `AWRCreatureAI::Tick` (test-control gate at `WRCreature.cpp:2602`) and `AWRBoss::Tick` (`WRBoss.cpp:961`); target
  picking `AWRCreatureAI::PickTarget` and `AWRBoss::PickBossTarget`.
- Achievements: `Unlock()` (`WRDeedsAchieve.cpp:95`); kill-time boards: `WRRecords::Note` (`WRRecords.cpp:195`).
- F8: `WRReport BuildContext` (`WRReport.cpp:152`).
- Seasons: not in the game (FULL-GAME.md:114, RAVEN-PLANNED Dream tier).

## 2. Plan (design, every gap solved)

### 2.1 Who: the proof chain

```
F35Update (links the PC once)            Freedom35 site                         Wildreach server / host
  pc-link.bin (DPAPI) ------------------> POST /api/pc/tester                     (listen host, dedicated, or SP)
Wildreach client reads it at start        requirePcLink -> account                  |
  1. status:  {game, bind}       ------->  game_testers row? -> {tester, label}      |
                                                                                    join: ClientGodChallenge(nonce)
  2. proof:   {game, bind, nonce} ------>  Ed25519-signed proof (10 min)  <-- client asks with that nonce
                                                                                    ServerGodProof(proof)
                                                                                    verify signature (public key compiled in),
                                                                                    aud, game, exp, bind == H(WRKey), nonce
                                                                                    -> UWRGodComponent.bTester (replicated)
```

- **Allow list**: D1 table `game_testers (user_id PK, label, games, added_at, note)`; rows inserted by the admin tool
  `tools/testers.mjs add --email <e> --label Aaron` (through `POST /api/admin/testers`, x-admin-token). No email in any
  file in either repo; the migration creates the table only. Colin: both of his accounts are added (label Colin); the
  question of which one he plays on goes to QUESTIONS-FOR-TOMORROW.
- **Status call** (no nonce): answers `{tester:false}` for everyone else, the same shape whether the PC is unknown to
  the list or not. Rate-limited per link (120/h).
- **Proof**: `base64url(payload) "." base64url(Ed25519 signature over "wrtester." + payload)`; payload
  `{v:1, iss:"freedom35studios", aud:"wildreach-tester", game:"wildreach", sub:<playerKey(secret, user, "tester")>,
  label, bind, nonce, iat, exp: iat+600}`. Signed with `TESTER_SIGNING_KEY` (PKCS8, a Pages secret; the only copy off
  Cloudflare is `~/.f35/tester-signing-key`). Every proof issued writes a `game_tester_log` row (who, which game, which
  PC link) for the audit.
- **bind** = SHA-256(WRIdentity token + "wildreach-tester-bind-1"): the site never sees the PC's token; the server
  recomputes it from the `?WRKey` the player joined with. A proof copied to another PC (another key.bin) fails.
- **nonce**: the server makes one per connection at login (`FGuid`, 32 hex) and sends it with `ClientGodChallenge`; a
  proof is good only for the connection whose nonce it carries, once. A host that captures a proof can't replay it on
  another server.
- **Verification on the server** (`WRGodAuth::Verify`): OpenSSL Ed25519 (engine's OpenSSL 1.1.1t,
  `EVP_PKEY_new_raw_public_key` + `EVP_DigestVerify`) against the public key(s) compiled into the game (two slots for a
  future rotation); checks iss, aud, game, `exp > now`, `iat <= now + 120`, bind, nonce. At most 4 proofs per connection
  (spam guard). Failures log `GOD refused proof from <name>: <reason>` (Warning).
- **Non-testers send nothing**: the client asks the site for status once per game start; only a tester answers a
  challenge. No pc-link.bin (game not run through Freedom35): no request at all.
- **Revocation**: removing a row stops the next proof (10-minute expiry; a tester flag lasts the connection).
- **Offline**: no proof without the site; god mode needs the testers to be online (they always are when hosted).

### 2.2 Gating everything else

- **God commands**: `god <verb> ...` through the existing `ServerCommand` RPC, intercepted at the top of
  `ServerCommand_Implementation` and run by `WRGod::Run(PC, Args)`, which checks the tester flag first every time.
  Non-tester: refused, `GOD refused '<cmd>' from <name>: not a tester` (Warning), nothing else happens.
- **The old dev commands** (`tp`, `give`, `spawn`, `hour`... and every `<system> dev ...`): in packaged builds
  (`!WITH_EDITOR`) they now need a verified tester too, and like god commands they're refused on public worlds. Editor
  builds (Raven's and the test scripts' `UnrealEditor.exe -game` runs) keep them as before. The dev list is explicit
  (`WRGod::IsDevCommand`): the commands the controller's own chain runs before and after `RunGameplayCommand`, plus
  any `<x> dev`. Gameplay commands are untouched.
- **Public worlds** (R60): a dedicated server whose preset isn't in `god.privatePresets` (tuning; default
  `["playtest"]`) and wasn't started with `-wrtestworld` is public: god and dev commands are refused there ("God mode is
  off on public worlds"), the panel says so. Single player, hosted (listen) worlds and playtest servers allow it.
- **No menu, no key, no command for others** (R7-R9): the panel's screen, the key, the pad stop and the badge all ask
  `WRGod::IsLocalTester(PC)` (the replicated flag) first; the key binding row is tester-only (hidden from the Controls
  page unless you're a tester); the `WRGod` Exec helper prints nothing and sends nothing for a non-tester.

### 2.3 The powers (each: what it does, where it hooks, what it meets)

State lives on `UWRGodComponent` (a component of `AWRPlayerState`, replicated to everyone: `bTester`, `Label`,
`Flags` bitmask, `FlySpeed`, `bCharacterTested`; to the owner: `bWorldTested`, `bAIFrozen`, `bPublicWorld`). The
server ticks it (0.25 s) for the vitals; the owning client and the server apply movement.

| Power | Server effect | Hooks |
|---|---|---|
| Invulnerable | blows still land (block, stagger, sparks, sounds play, so combat stays testable) but health never drops; a refused `Die` puts health back | `ApplyHit` final damage = 0 for the target (`WRCombatLibrary.cpp` before the health write); `AWRCharacterBase::Die` refuses for an invulnerable survivor |
| Endless vitals | every 0.25 s: stamina, all 11 path bars (Heat, Chill, Charge, Growth, Momentum, Souls, Mana, Flow, Radiance, Spores, Void) to max, hunger / thirst 100, corruption 0, burn / frost build-up 0, max-health lost 0; ability cooldowns not touched | god tick |
| One-hit kill | a blow from you that lands takes the target's whole health (after its own guard / parry / dodge rules decided it landed) | `ApplyHit` before the health write |
| Fly | movement mode Flying on server and owner; Move uses the camera's pitch forward (like underwater), Jump rises, Dive (Ctrl) / pad B held sinks; speed 600 cm/s x `FlySpeed` (0.5-10, panel slider) | `AWRCharacter::Move`, `InputJump`, `InputPressed(Dodge)`, god tick re-asserts the mode |
| Pass through walls | Fly + the capsule's collision off on every machine | god tick / OnRep |
| Unseen | creatures and bosses never pick you as a target, and drop you if they had you | `AWRCreatureAI::PickTarget`, `AWRBoss::PickBossTarget`, the boss's wake check |
| Heal / Clear statuses / Revive here | full health; statuses cleared; dead: wake at once where you fell | `AWRGameMode::Wake` + teleport back |
| Kill all nearby | every living wild creature / boss within `god.killRadius` (40 m), not tames, killed by you (XP, loot and quests flow as for any kill) | `ApplyHit` burst then `Die(you)` if still standing |
| Freeze AI | every creature's and boss's brain stops (no moving, no new attacks), world-wide; off resumes | `AWRCreatureAI::Tick`, `AWRBoss::Tick` |
| Hitboxes and aggro (View) | local only: capsules of everything within 30 m (players green, tames blue, wild amber, hunting you red) and a line from each creature to who it's after | debug draw in the god component's owner tick (Development builds draw it; Shipping has no debug draw) |

Travel, Creatures, Items, Progress, World, Quests (each a panel tab):

- **Travel** (R30-R34): the server builds the list from what exists: every live land (`world.json biomes` centre,
  settled onto ground), every boss (`FWRPoi` boss rows: the arena), every shrine and Great Shrine (`AWRShrine`, found or
  not), villages, camps, caves, passes and landmarks (`FWRPoi`), portals (`AWRPortal`), the other players, and your map
  waypoint (sent by your client). Rows carry kind, land and distance; a search box and kind chips filter it. Go = the
  server resolves the row it sent (no free coordinates except your waypoint), drops you onto the ground there (a down
  trace from 2 km, or from just above a place's own height for caves).
- **Creatures** (R35-R36): every row of `creatures.json` (207: wildlife, minis, majors, portal bosses), filtered by land
  chips and a search box, grouped by tier; amount 1 / 3 / 5 / 10; Spawn (in front of you, spread on the ground, facing
  you) and Spawn tamed (not bosses: `CompleteTame` to you).
- **Items** (R37, R38, R41-R43): every item (1082) by category chips and search, 60 rows at a time; amount 1 / 10 / 50 /
  a full stack; Give. Armour sets (`gear.json sets`, 12): Give set / Give Ascendant set (the four `<set>_<slot>` items).
  Recipes: any recipe's result or its materials (x1 / x5). Unlock all: learn every blueprint (`KnownBlueprints`), the
  nearest Hearth to tech 5 (`hearthlevel`), a builder's kit (500 of every material any building piece costs, from
  `pieces.json`).
- **Progress** (R39, R40, R44): the 12 paths (main / subclass, +10 points, max (100: the fixed per-path cap), unlearn;
  every ability node is bought by the max in tree order), the life skills (level -/+ and max), character level shown.
- **World** (R45-R48, R52): hour (dawn 6, noon 12, dusk 18, midnight 0, and -1 / +1 h), weather per land (each kind of
  that land from `weather.json`, Clear), every event in `world.json events` (Start), invasions (each pair, Start), Freeze
  AI. Seasons: not in the game (Dream tier), so nothing shows for them.
- **Quests** (R49): each master's quests by name with your step; Back a step, Complete this step, Reset, Finish all;
  Find every shrine.

### 2.4 Panel (R54-R56)

- `EWRMenu::God` -> screen "god" (`WR_REGISTER_SCREEN`), the stone window kit (`WRUIWindowKit::Window`, 1240x760):
  badge = an oxblood disc with a brass rim and a gold eye-in-sun sigil; title "God mode"; subtitle "Tester tools ·
  <label> · only Freedom35's developers have this"; chip: "Testing world" (green) / "Public world: off" (red) / "World
  marked tested" (amber).
- Body: left rail of seven tabs (Self, Travel, Creatures, Items, Progress, World, Quests) as leather buttons with the
  open one lit; right: the page in a scroll box. A banner across the top until the character is marked: "Using anything
  here marks <name> as a test character for good: no achievements or kill-time records from it. World changes (time,
  weather, events, spawns, freezing) mark this world too." (Minecraft / Satisfactory: say it before, not after.)
- Toggles are rows: a lit switch, the name, one plain line of what it does. Every clickable piece is an `SWRButton` or
  a button with the hand cursor, so the pad's cursor drives it (WRPadInput); search boxes open the pad keyboard.
- Hotkey: **F9** (`GodMode` binding, tester-only row in Settings > Controls); on a pad, God is the last stop of the
  journal's LB / RB page cycle for testers. Esc / B / the X closes it. It opens while dead too (Revive).
- **Badge** (R56): while any power is on (or AI frozen), a tab on the left screen edge at mid-height (free of the vitals,
  chat, compass and plates), over every screen (Z 60): oxblood leather, brass edge, "GOD" in gold Cinzel, under it the
  powers on as small caps words (INVULNERABLE, ENDLESS, ONE-HIT, FLY, NOCLIP, UNSEEN, AI FROZEN). Others see "GOD" in
  red-gold over the tester's name (Rust's admin tag).

### 2.5 "Just for testing" (R57-R61)

- **Marks**, kept in `Saved/Worlds/<world>/tested.json` by the server: the character (first use, by whom, count of
  uses) on any god command; the world on world-changing ones (hour, weather, event, invasion, freeze, spawn, kill all,
  shrines, hearth). Marks never clear.
- **Achievements**: `Unlock` skips a tested character (logged). **Kill-time boards**: `WRRecords::Note` skips a kill
  any tested character took part in (logged). (Wildreach has no Freedom35 SDK achievements or leaderboards yet; when it
  does, the same flag gates them.)
- **Public worlds**: refused (2.2), so they stay clean.
- **F8**: the report's context gets `godMode {tester, label, on: [...], characterTested, worldTested}` whenever the
  reporter is a tester or anything is marked; the inbox's trimmed context keeps it.

### 2.6 Numbers (Data/tuning.json `god`)

`site`, `tickSeconds` 0.25, `flyBase` 600, `flySpeedMin` 0.5, `flySpeedMax` 10, `killRadius` 4000, `hitboxRange` 3000,
`spawnAhead` 700, `privatePresets` ["playtest"], `buildKit` 500, `listMax` 60, `proofsPerConnection` 4.

### 2.7 Build steps

1. Site: migration (tables), `_lib/testers.js` (allow list lookup, proof signing), `api/pc/tester.js`,
   `api/admin/testers.js`, `tools/testers.mjs`, README section; apply the migration; add the secret; add the rows;
   deploy with `npm run deploy`; check the gate (`/play/corelode` still 303) and the endpoint (401 without a link).
2. Game core: `WRGodAuth.h/.cpp` (client: read pc-link.bin with DPAPI, status + proof requests; server: verify),
   `WRGod.h/.cpp` (component, commands, marks, gates, tick, movement), Build.cs (OpenSSL), PlayerState subobject,
   controller RPCs (challenge / proof) and the `god` intercept + dev gate, GameMode (WRKey + challenge), hooks in
   ApplyHit / Die / creature + boss AI / PickTarget / Move / Jump / Dodge, Deeds Unlock, Records Note, F8 context,
   nameplate tag, tuning rows. Compile. Commit + push.
3. Panel + badge + key + pad stop + hitboxes. Compile. Commit + push.
4. Spot-check against this plan, fix, CHANGELOG / HANDOFF, ledger proofs, LANDED.

## 3. Spot-check (against the plan and the BAR, by reading the code)

Found and fixed before the second push (f7c7623): the search box sat inside the list that rebuilds as you type (lost
focus every keystroke) -> a fixed head above the list; the panel (Z 20) was under the death cover (Z 60), so Revive
was unreachable dead -> god screen at Z 65, badge at Z 70; Revive needed a possessed body -> the server keeps where you
last stood alive; Endless vitals pinned Heat / Chill / Charge at full, which fires Overheat on every gain -> only the
spending bars fill; Travel missed the island's non-landmark named places and any boss whose arena the map didn't name ->
added; the main menu's backdrop world asked for a proof at every start -> skipped; a non-tester rebinding F9 would
have seen "God mode (testers)" in the clash message -> tester-only rows never clash for them. Compile clean (editor
target, adaptive and unity blobs).

## 4. Questions for tomorrow (my pick built)

- Colin has two Freedom35 accounts named Colin Carson (a creator account from 2026-10-04 and a player account from
  2026-11-09). Both are on the list (label Colin). Alternative: keep only the one he plays on (`node tools/testers.mjs
  remove --user <id>`).
- Public worlds: god mode is refused on a dedicated server whose preset isn't `playtest` (unless started with
  `-wrtestworld`). Alternative: allow self-only powers there (invulnerable, fly) with the character still marked.
- The old testing commands (`tp`, `give`, `spawn`...) now need a verified tester in packaged builds; until today any
  player's console could run them. Alternative: leave them open (they were, by accident).

## 5. Ledger proof (commits: site 7098076 freedom35-studios; game 5aa7e69 + f7c7623 Wildreach; doc a317f21)

| # | Done | Where |
|---|---|---|
| R1 | yes | WRGod.h/.cpp, WRGodAuth.h/.cpp, WRUIGod.cpp (5aa7e69) |
| R2 | yes | site `functions/_lib/testers.js` testerFor + `api/pc/tester.js`; game WRGodAuth.cpp:172 Verify, WRGod.cpp:651 ReceiveProof |
| R3 | yes | tester only by a site-signed proof (WRGodAuth.cpp:36 public key, :68 EVP_DigestVerify); no name / config / flag path anywhere (`-wrtestworld` only marks a server private, grants nothing) |
| R4 | yes | game_testers row "Aaron" (tools/testers.mjs list, 2026-11-16 03:01 UTC) |
| R5 | yes | game_testers row "Raven" |
| R6 | yes | both Colin Carson accounts, rows "Colin" (question 4.1) |
| R7 | yes | WRPlayerUI.cpp:212 (screen maps to nothing), WRPlayerController.cpp:1202 ToggleGod, WRUIGod.cpp Tick closes it |
| R8 | yes | WRPadInput.cpp:533 (F9 only for a tester), WRUISettings.cpp:1191 / WRUIControls.cpp:194 (row hidden), WRKeyBindings.cpp:124 (no clash), WRPlayerUI.cpp:368 (no pad stop) |
| R9 | yes | no new Exec; `god ...` by any route refused: WRGod.cpp:698; old dev commands gated WRPlayerController.cpp:1150 |
| R10 | yes | WRGod.cpp:617 Block, :698 refusal |
| R11 | yes | WRGodAuth.cpp:76 ReadPcLink (the updater's existing pc-link.bin); no new sign-in UI |
| R12 | yes | site `requirePcLink` (pclinks.js), `playerKey` (sdk.js) reused in testers.js |
| R13 | yes | site `game_testers` + `api/pc/tester.js` |
| R14 | yes | site `mintTesterProof` (Ed25519, TESTER_SIGNING_KEY); game WRGodAuth.cpp:172 |
| R15 | yes | proof `sub` = playerKey(account, "tester"); label from the row |
| R16 | yes | `bind` = SHA-256(WRIdentity token + salt): WRGodAuth.cpp:221 |
| R17 | yes | `exp` 10 min: WRGodAuth.cpp:219 |
| R18 | yes | reads exactly what F35Update writes (DPAPI CurrentUser pc-link.bin); no Wildreach game row needed on the site |
| R19 | yes | deployed with `npm run deploy` (39cff9d7); gate 303; endpoint 401 unlinked; secret set via stdin, never printed |
| R20 | yes | WRGameMode.cpp:685 Challenge at login, :142 join key; WRPlayerController.h:229 ServerGodProof; bTester replicated WRGod.cpp:291 |
| R21 | yes | WRPlayerController.cpp:1146 -> WRGod.cpp:687 Run re-checks Block on every call; refusals logged :698 |
| R22 | yes | section 1.1 (Valheim, ARK, Elden Ring, Minecraft, Terraria, Satisfactory, 7DTD, Rust) |
| R23 | yes | WRCombatLibrary.cpp:948, WRCharacterBase.cpp:235, WRGod.cpp:375 tick |
| R24 | yes | WRGod.cpp:389 (stamina) |
| R25 | yes | WRGod.cpp:389 (the spending path bars; the builders left to the game, section 3) |
| R26 | yes | WRGod.cpp:389 (hunger, thirst) |
| R27 | yes | WRCombatLibrary.cpp:947 |
| R28 | yes | WRGod.cpp:312 ApplyMovement, WRCharacter.cpp:1777 / :1826 / :1891, WRCharacterMovementComponent.cpp:51, speed WRGod.cpp:725 |
| R29 | yes | WRGod.cpp:312 (collision off on every machine) |
| R30 | yes | WRGod.cpp:95 BuildPlaces (world.json biomes, the live lands) |
| R31 | yes | WRGod.cpp:95 (FWRPoi, island_places, hidden tubes, portals, players; waypoint :793) |
| R32 | yes | WRGod.cpp:95 (AWRShrine actors + world.json shrines list) |
| R33 | yes | WRGod.cpp:95 (FWRPoi boss rows + standing bosses' homes) |
| R34 | yes | BuildPlaces reads only data and actors; nothing hand-typed |
| R35 | yes | WRGod.cpp:812, WRUIGod.cpp:524 (207 creatures, x1-10, tamed) |
| R36 | yes | same (the 62 bosses, listed first) |
| R37 | yes | WRGod.cpp:833, WRUIGod.cpp:593 (1082 items: 1 / 10 / 50 / a stack) |
| R38 | yes | WRGod.cpp:846, WRUIGod.cpp:652 (result or materials) |
| R39 | yes | WRGod.cpp:885 (any of the 12 paths as main / subclass, the game's rule), WRUIGod.cpp:695 |
| R40 | yes | WRGod.cpp:885 "max" (100 points, AutoSpend every node in tree order) |
| R41 | yes | WRGod.cpp:862 blueprints + :879 Hearth tech 1-5 |
| R42 | yes | WRGod.cpp:871 builder's kit + :879 Hearth tech |
| R43 | yes | WRGod.cpp:840, WRUIGod.cpp:631 (12 sets, Ascendant too) |
| R44 | yes | WRGod.cpp:885 points, :903 life skills; character level shown WRUIGod.cpp:695 |
| R45 | yes | WRGod.cpp:909, WRUIGod.cpp:738 |
| R46 | yes | WRGod.cpp:918 (UWRWeatherSubsystem::Force), every land's kinds WRUIGod.cpp:738 |
| R47 | n/a | seasons aren't in the game (FULL-GAME.md:114, Dream tier); nothing shown (LATER-COOKS line) |
| R48 | yes | WRGod.cpp:927 (events, invasions through WRWorldSystems::Dev) |
| R49 | yes | WRGod.cpp:927 quest, WRUIGod.cpp:809 (back / complete / reset / finish) |
| R50 | yes | WRGod.cpp:748; reachable dead (WRPlayerUI.cpp:252, Z 65) |
| R51 | yes | WRGod.cpp:762 (40 m, as your kills) |
| R52 | yes | WRGod.cpp:937, WRCreature.cpp:2608, WRBoss.cpp:971 |
| R53 | yes | WRGod.cpp:415 DrawHitboxes, WRUIGod.cpp:447 |
| R54 | yes | WRUIGod.cpp:182 (window kit, seven pages, buttons for the pad, the pad keyboard on search) |
| R55 | yes | F9 WRKeyBindings.cpp:97 / WRPadInput.cpp:533; pad: the last journal page WRPlayerUI.cpp:368 |
| R56 | yes | WRUIGod.cpp:849 SGodBadge, :885 TickBadge (Z 70); for others WRHUD.cpp:488 |
| R57 | yes | WRGod.cpp:493 Mark (tested.json: the character always, the world on world changes) |
| R58 | yes | WRDeedsAchieve.cpp:102 |
| R59 | yes | WRRecords.cpp:202 |
| R60 | yes | WRGod.cpp:590 IsPublicWorld, :617 Block (god and dev commands refused) |
| R61 | yes | WRReport.cpp:219 godMode (kept in the inbox's compact copy) |
| R62 | yes | the migration creates tables only; rows via the admin API; no email in either repo or any commit message |
| R63 | yes | private key only in ~/.f35 and the Pages secret; the PC token goes as a header, never logged |
| R64 | yes | sections 0-2 written before building (a317f21), 3 and 5 after |
| R65 | yes | Build.bat WildreachEditor: 0 errors, 0 warnings (build logs 4-9) |
| R66 | yes | CHANGELOG "Unreleased: the testers' god mode" + HANDOFF paragraph in 5aa7e69 (and f7c7623) |
| R67 | yes | site 7098076, game 5aa7e69, f7c7623: each pulled / rebased and pushed |
| R68 | yes | LANDED.md line; QUESTIONS-FOR-TOMORROW.md (section 4) |
| R69 | yes | /play/corelode 303 after the deploy |
| R70 | yes | Data/tuning.json:1000 `god` |
