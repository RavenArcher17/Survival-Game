# Wildreach: notes for build sessions

Multiplayer open-world survival, UE 5.8, C++. Read this before working on the project.

## Design sources (two separate docs)
- **Build plan** (scope and order): https://claude.ai/artifact/RexKaP5rCQmTdw3vmUfzbq
- **Design doc** (what each system is): https://claude.ai/artifact/VQd91XUksbVNsMTZx8EP7k

They're separate docs. **Scope = the build plan's First Playable tier: Fire and Frost only** (Volcanic + Frozen tundra).
Don't build anything from the other 10 biomes/paths, or from the Full release / Dream list tiers, unless Raven asks.
Read both through the Claude Docs connector; re-read before each milestone, since Raven keeps editing them.

## Things that are easy to miss
- **Everything in the docs is a first draft**: names, perk effects, combos and starting numbers especially. They're meant to be tuned in playtests, not treated as final. Keep them in data files (`Data/*.json`, `Data/tuning.json`) so changing them never needs a code change, and don't over-engineer around a specific value.
- **The Map tab layout isn't ready to build as-is.** Its note says the layout needs more planning toward a realistic landmass (coastlines, terrain, rivers, mountain range shape) first. M3 starts with that planning step and gets Raven's sign-off before any terrain is built.
- **Playtests run with XP turned up.** At 1Ã— a path takes ~80 h and a life skill ~50 h, so no one would reach the higher nodes. Playtest servers use the `Playtest` server preset (high XP/skill multipliers, see `Docs/plan/M7_server.md`), and debug commands can grant points directly.
- Open inconsistencies between doc tabs are in `Docs/DESIGN_REVIEW.md`. Don't silently pick an answer: use the proposed default listed there and leave the item open for Raven.

## Plan
- `Docs/TECH_PLAN.md`: architecture decisions + milestone index
- `Docs/plan/M1_combat.md` â€¦ `M7_server.md`: per-milestone tasks, gates, tests
- `Docs/plan/DATA_SCHEMAS.md`: JSON formats for design content

## Status (2026-10-27)
- **Polish pass (0.4.0)**: sound and effects are named events (`WRSound::PlayAt/PlayOn/Voice/Broadcast`,
  `WRFx::SpawnAt/Broadcast/ShowHit`); what plays is data (`Data/Audio/sounds.json` + OGG files loaded at runtime,
  `Data/fx.json` recipes drawn by the C++ sprite system in `WRFx*`), so new moments need a table row, not code.
  Animation is `UWRAnimInstance` (C++ layered blending, no animation blueprint; clips from `player_art.json` /
  `creatures.json`, numbers in tuning `anim`). Ambient beds and music: `UWRAudioDirector`. World look: `WRSea`,
  `WRSky` (biome air, valley mist), `WRBuildLook` (build-piece detail over the unchanged collision blocks),
  `WRCameraBoom`. Death/loading/moments/hints: `WRUIFlow`, `WRFlowDeath`, `WRUIMotion`. Far creatures tick in slices
  (`UWRSignificanceSubsystem`); art preloads (`UWRPreloadSubsystem`, `Data/preload.json`). Measure with
  `Tools\benchmark.ps1` (results in `Docs/PERFORMANCE.md`); verify with `Tools\uishots.ps1`, `animshots.ps1`,
  `fxshots.ps1`, `worldshots.ps1`, `audiotest.ps1`, `flowtest.ps1`. Third-party files and licences:
  `Docs/CREDITS.md` (shipped beside the game).
- **UI: the stone-and-leather Slate UI** (2026-10-26, from Raven's approved mockups): `FWRPlayerUI` per local player
  (`WRPlayerUI`), style + widget kit (`WRUIStyle`, `WRUIWidgets`, `WRUIWindowKit.h`), every key in `FWRKeyBindings`
  (Esc → Controls). Screens and journal tabs live in their own `WRUI*.cpp` and register with
  `WR_REGISTER_SCREEN` / `WR_REGISTER_JOURNAL_TAB`; they show while the controller's `EWRMenu` matches and send the same
  server commands. `-oldui` keeps the old text HUD (`WRHUD::DrawMenuPanel`), which still draws menus with no screen.
  `Tools\uishots.ps1` screenshots them all. Slate gotchas: images/fonts load from `Data/UI` at runtime (PNG through
  `IImageWrapper` into `FSlateDynamicImageBrush`; an `FSlateImageBrush` file path renders white); mockup colours are
  sRGB, so build them with `FColor` (linear literals wash out); `FSlateDrawElement::MakeBox` ignores the brush tint
  (`WRUI::DrawBox`); a tiled `SImage` needs `DesiredSizeOverride(1,1)` or it sizes to the texture;
  `SetIgnoreLook/MoveInput` are counters (reset, then set once); unity builds join UI files, so share `WRUI::U` and keep
  helpers in named namespaces; screenshots of Slate need `shot showui` (HighResShot leaves UI out).
- **First playable is feature-complete**; art pass 2026-10-24/25: every creature but the training dummy has art (AI
  pipeline; the 3 humanoid bosses rigged on Mixamo, Ignessa a winged fae rig, the wisp a glowing orb), the player is a
  Mixamo-rigged male or female survivor (menu choice, `Data/player_art.json`) with an AI sword and shield.
- Every First Playable row of the build plan has a working version; what's left is playtesting/tuning, a packaged
  dedicated server and HLODs.
- **M1 combat core: done.** **M2 paths: done** (persistence v1 = JSON per player). Paths menu (P) learns nodes.
  Built ability nodes: bolts/shards, weapon arts, Fire wall, Lava pool, Frost nova, Ice lance; other ability nodes
  (Meteor, Ice wall, Blizzard, Flurry...) show "[not built yet]" in the menu.
- Front end: `WRMainMenu` (standalone only): name, path, Host (listen server, UDP 7777) / Join by IP; F10 back.
  `Tools\package.ps1` packages a shareable Windows build (Development) with the launcher engine. A packaged
  dedicated server needs the source engine: `Tools\build_engine.ps1` (clears `NoDefaultCurrentDirectoryInExePath`,
  which otherwise breaks Epic's batch files), log `C:\Dev\UE5Source-build.log`.
- M3: layout approved 2026-10-23; **`L_Island` built** (WP landscape, default map). Rebuild with `Tools/terrain/build_island.ps1 -Force` (runs in the full editor: the landscape's visible textures need the GPU layer update, which commandlets don't do).
  Dressing/caves/lava+ice cells: `node Tools/terrain/dressing.mjs` -> `Data/island_dressing.json`, built at runtime by
  `UWRDressingSubsystem` (no map rebuild). Decoration culls at ~250 m (WP loading range). Next: HLODs.
  Materials (`M_IslandAuto`, `M_Rock`, `M_Lava`, `M_Ice`): `Tools/terrain/build_terrain_materials.py` (commandlet, rebuilds in place).
  The dressing uses pack meshes (SimpleRocks, GV shrubs, AI trees in `/Game/Props`) and falls back to engine shapes if a pack is missing.
- Art: creatures and props come from the local AI pipeline (`Tools/art/README.md`; env + models in `C:\Dev\WildreachArt`,
  not in git). Fab packs live in `Content/<pack>` and are git-ignored; adding a "complete project" pack can overwrite
  `Config/Default*.ini` and drop a `.uproject` in the root, so check `git diff Config/` after adding one.
  Anything loaded by path (creature art, dressing meshes) must be under a `DirectoriesToAlwaysCook` entry in `DefaultGame.ini`.
  Cooking crashes (`Assertion failed: !Hash.IsZero()`) on any skeleton-less clip in a cooked folder: the Barghest pack's
  MANE/TAIL clips and meshes are moved out to `C:\Dev\WildreachArt\replaced\barghest_parts`; re-adding the pack brings them back.
- 2026-10-25 survival loop pass: the island's trees / rocks / shrubs are the gathering (`AWRDressing::HarvestInFront`,
  used-up ones replicate via `AWRHarvestLedger`), tools work in the hand since 0.5 (weapon-slot equipment: the held one
  sets the yield and wears, `FWRGathering`, drawn from engine shapes in `WRHeldTool.cpp`; wild kills stay down as bodies
  butchered hit by hit, `WRCarcass.cpp`; numbers in tuning `gather`, `Tools\gathertest.ps1`),
  crude tools are made by hand (recipes with station `hand`, top of the B menu), stone tools at the workbench.
  `Tools\survivaltest.ps1` plays the fresh-player loop on L_Island (backs up and restores the island world save).
- 2026-10-23 "build the rest of the first playable" run, in batches (each tested + committed):
  1. Crafting & equipment: done (`WRCrafting`, `WRInventory`, menus in `WRPlayerMenus.cpp`; `Tools\crafttest.ps1`).
  2. Building: done (`WRBuilding`, build mode in `WRPlayerBuild.cpp`, world save in `WRGameMode`; `Tools\buildtest.ps1`).
  3. Creatures & taming: done (`WRCreature`; `Tools\tametest.ps1`). 4. Bosses: done (`WRBoss`; `Tools\bosstest.ps1`).
  5. Life skills / server presets / clans / safe zones / PvP / death penalty: done (`WRLifeSkills`, `WRSocial`,
     `Data/lifeskills.json`, `Data/server_presets.json`, `-wrpreset=`; `Tools\socialtest.ps1`). All checks pass. Fishing/engineering/swimming have no XP source yet; bounties/raids settings unused.
  6. Island dressing, caves, biome hazards (`WRWorldDressing`; `Tools\terrain\hazardtest.ps1`): done. Island rebuilt 2026-10-24.
  Default preset is `playtest` (PvP off), so PvP tests need `-wrpreset=pvp_standard` (mptest and socialtest pass it).
- GitHub backup: origin = https://github.com/RavenArcher17/Survival-Game (Git LFS for assets; free LFS quota 1 GB, ~770 MB used 2026-10-25 (each Mixamo character ~30-40 MB); each island rebuild adds ~80 MB, each AI creature ~11 MB).
- See `CHANGELOG.md` for details and known gaps.

## How things are wired (read before changing combat)
- All damage goes through `UWRCombatLibrary::ApplyHit` on the server. Don't apply damage anywhere else.
- Statuses are replicated loose tags (`AddLooseGameplayTag(..., EGameplayTagReplicationState::TagOnly)`), owned by `UWRStatusComponent`.
- Movement speed changes the client predicts (sprint, attack/block slow) are CMC saved-move flags; server-driven ones (slowed, frozen, staggered) come from tags.
- New path ability = a `UWRGameplayAbility` subclass + an entry in `UWRAbilityRegistry::ClassForNode`.
- Numbers go in `Data/tuning.json`, read with `WRTune(TEXT("a.b.c"), fallback)`.

## Build & test
```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" WildreachEditor Win64 Development -Project="C:\Dev\Wildreach\Wildreach.uproject" -WaitMutex
Tools\play.ps1 local -Clients 2     # dedicated server + 2 windowed clients
Tools\make_map.ps1 [arena] [-Force] # regenerate a test map headless
Tools\test.ps1                      # automation tests, must stay green
Tools\mptest.ps1                    # server + bots; check hits/statuses/abilities/errors
Tools\persisttest.ps1               # save/restart/load round trip
Tools\shots.ps1                     # rendered screenshots (look at them: verify visuals from 2+ angles)
```
Headless clients must run with `t.MaxFPS 60` or they flood the server with moves. Without an online subsystem,
use `?WRName=` for a stable player name. Runtime errors go to the log, not the exit code, so check logs for `Error:`.
Don't close Raven's open editor or game windows; launch new ones alongside.

## Conventions
- Server-authoritative everything; clients send intent, never results.
- Gameplay logic in C++ and data, not Blueprint-only assets, so it can be built and reviewed without the editor UI.
- Class prefix `WR`. Commit to `master` (no feature branches). Large binaries go through Git LFS (`.gitattributes`).
