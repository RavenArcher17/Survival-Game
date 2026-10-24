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

## Status (2026-10-22)
- **M1 combat core: done** on placeholder art. **M2 paths: done** (persistence v1 = JSON per player).
- Stat nodes work (tuning `nodeEffects`); ability nodes past the trunk (Fire wall, Meteor, Ice lance...) aren't built. No creation screen or skill-tree UI yet.
- M3: layout approved 2026-10-23; **`L_Island` built** (WP landscape, default map). Rebuild with `Tools/terrain/build_island.ps1 -Force` (runs in the full editor: the landscape's visible textures need the GPU layer update, which commandlets don't do). Next: textured auto-material, PCG dressing, caves.
- 2026-10-23 "build the rest of the first playable" run, in batches (each tested + committed):
  1. Crafting & equipment: done (`WRCrafting`, `WRInventory`, menus in `WRPlayerMenus.cpp`; `Tools\crafttest.ps1`).
  2. Building: done (`WRBuilding`, build mode in `WRPlayerBuild.cpp`, world save in `WRGameMode`; `Tools\buildtest.ps1`).
  3. Creatures & taming: done (`WRCreature`; `Tools\tametest.ps1`). 4. Bosses: done (`WRBoss`; `Tools\bosstest.ps1`).
  5. Life skills / server presets / clans / safe zones / PvP / death penalty: done (`WRLifeSkills`, `WRSocial`,
     `Data/lifeskills.json`, `Data/server_presets.json`, `-wrpreset=`; `Tools\socialtest.ps1`). All checks pass. Fishing/engineering/swimming have no XP source yet; bounties/raids settings unused.
  6. Island dressing (caves, foliage), then rebuild the island, full regression, docs, push. Default preset is `playtest`
     (PvP off), so PvP tests need `-wrpreset=pvp_standard`.
- GitHub backup: origin = https://github.com/RavenArcher17/Survival-Game (Git LFS for assets; free LFS quota 1 GB). Epic source access works (UE source build not started).
  After all batches: rebuild the island (`Tools\terrain\build_island.ps1 -Force`) so it gets the new nodes/camps.
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
