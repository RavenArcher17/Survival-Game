# Wildreach (working title)

Multiplayer open-world survival, first + third person, UE 5.8, C++. The first playable (fire + frost, Volcanic +
Frozen tundra) runs on placeholder art. See `CHANGELOG.md` for what's built and its known gaps.

## Play with friends

```
Tools\package.ps1          # -> C:\Dev\WildreachBuild\Windows\Wildreach.exe (+ README.txt for players)
Tools\make_installer.ps1   # -> C:\Dev\WildreachBuild\Wildreach-Setup.exe (one file to send friends)
```

Send `Wildreach-Setup.exe` with a OneDrive/Drive link (it's ~440 MB). Double-clicking it installs to the player's
own folder (no admin), adds Desktop and Start Menu shortcuts, installs the VC++ runtime only if it's missing, and
starts the game; a newer Setup updates in place and keeps saves. It isn't code-signed, so Windows may show "Windows
protected your PC": More info, then Run anyway. Everyone runs the game, types a name, picks a
Male or Female body and Fire or Frost. One person clicks **Host** (their PC is the server; over the internet they forward UDP 7777
to their PC). Everyone else types the host's IP and clicks **Join**. Saves live on the host
(`Wildreach\Saved\ServerDB`). F10 goes back to the menu.

A packaged **dedicated server** needs Unreal built from source: `Tools\build_engine.ps1` (several hours,
~200 GB; log `C:\Dev\UE5Source-build.log`). Until then the host plays and hosts at once.

Server settings: the default preset is `playtest` (10x XP, PvP off). Pick another with `-wrpreset=pvp_standard`
(or `pve`, `casual_coop`), or write `Saved/ServerDB/server.json` with `{"preset": "custom", "base": "pve",
"settings": {...}}` using the keys in `Data/server_presets.json`.

## Build

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" WildreachEditor Win64 Development -Project="C:\Dev\Wildreach\Wildreach.uproject" -WaitMutex
```

Or double-click `Wildreach.uproject` (it offers to build if binaries are missing).

## Play from the editor build

```
Tools\play.ps1 local -Clients 2   # dedicated server + 2 windowed clients
Tools\play.ps1 listen             # host-and-play
Tools\play.ps1 server             # dedicated server only (port 7777)
Tools\play.ps1 client             # join 127.0.0.1:7777
```

The default map is `/Game/Maps/L_Island`, the 4 × 4 km island. Combat testing happens on `L_CombatArena`
(`play.ps1 -Map arena`). `play.ps1 -Path frost -Name You` picks your main path and keeps your save
(`?Path=` / `?WRName=` / `?Body=f` on the join URL). The main menu only appears when the game runs on its own (`-nomenu` skips it).

Rebuild the island: `Tools\terrain\build_island.ps1 -Force`. Change the trees/rocks/caves/hazards without a rebuild:
`node Tools/terrain/dressing.mjs` (writes `Data/island_dressing.json`).

### Controls

| Key | Action |
|---|---|
| WASD / mouse | move / look |
| LMB / R | light / heavy attack |
| RMB (hold) | block; the first moment of a block is a parry |
| C | dodge roll (i-frames) |
| 1–8 | path abilities in your quick slots |
| Q / Z / X | potion / eat / drink |
| Tab | lock on |
| E | interact: stations, chests, doors, beds, crops, pens, Hearth, feed or order creatures, nests, loot bags |
| I / P / K / B | inventory / paths (learn nodes) / life skills (perks) / build menu (also crafting by hand) |
| LMB at a tree / rock / shrub | gather wood / stone / fiber (a hatchet or pick in your bag gathers more) |
| G | swap sword-and-shield for the ranged weapon (bow, crossbow, blowgun, spears, bolas) and back |
| with it out: RMB (hold) / LMB / wheel | aim over the shoulder / fire or throw at the crosshair / next ammo (stone, tranq) |
| Shift / Space / V | sprint / jump / first-third person |
| M / T | map list (bosses, caves, camps, passes: distance + direction) / chat |
| F10 | back to the main menu |

### Dev commands (console)
`WRGivePoints fire 30`, `WRAutoSpend fire 30`, `WRLearn fire fire_wall`, `WRCast 3`, `WRSpawn cinderfang 3`,
`WRSpawn vulkarn 1`, `WRBossDmgPct 30 frost`, `WRGive iron_ore 20`, `WREquip copper_sword`, `WRWeapon iron fine 6`,
`WRSkill mining 50`, `WRTameStat`, `WRTorpor 100`, `WRClan create Name`, `WRHazard`, `WRHeal`, `WRTeleport x y`,
`WRGather tree` (bot gathers: tree / rock / shrub / node / any / off),
`WRGive hunting_bow 1`, `WRGive tranq_arrow 20`, `WRRanged 1`, `WRHold 30` (root nearby creatures), `WRClearCreatures`,
`WR.ReloadTuning` (re-read `Data/tuning.json` live). Tests can override numbers: `-wrtune=key:value,key:value`.

## Test

```
Tools\test.ps1          # automation tests (rules + deterministic combat scenarios)
Tools\mptest.ps1        # headless server + bots, PvE (-Scenario pvp for duels)
Tools\persisttest.ps1   # save -> server restart -> load
Tools\crafttest.ps1     # smelt, craft, infuse, upgrade, Hearth tiers, sockets, every ranged weapon + ammo, bow wear + repair
Tools\buildtest.ps1     # claims, code locks, raids, collapse, world save
Tools\survivaltest.ps1  # fresh player on the island: gather bare-handed, crude tools, hut + workbench, stone tools
Tools\tametest.ps1      # offer food, ride, knockout + feed, eggs, orders, tames saved
Tools\rangedtest.ps1    # every ranged weapon at a rooted Cinderfang: hits, tranq knockout, bola, spear pickup (-Body f)
Tools\bosstest.ps1      # all six bosses: phases, counterplay, party scaling, 10% loot rule
Tools\socialtest.ps1    # PvP preset, clans, Souls-like death bags, life skills + perks
Tools\spelltest.ps1     # Paths menu learn + the four tree spells
Tools\terrain\walktest.ps1 / hazardtest.ps1 / island_shots.ps1   # island traversal, lava + ice, screenshots
```

## Layout

| Path | What |
|---|---|
| `Data/tuning.json` | every gameplay number (starting numbers + placeholders) |
| `Data/server_presets.json` | server settings presets |
| `Data/paths/*.json`, `lifeskills.json` | skill trees, life skills |
| `Data/creatures.json`, `items.json`, `recipes.json`, `pieces.json` | roster + bosses, items, recipes, building pieces |
| `Data/island_dressing.json` | trees, rocks, caves, lava/ice cells (generated) |
| `Source/Wildreach/WRCombatLibrary` | the damage pipeline (block/parry/statuses/combos) |
| `Source/Wildreach/WRAbilities` | all GAS abilities |
| `Source/Wildreach/WRPathTypes`, `WRPathComponent` | trees, rules, progress, grants |
| `Source/Wildreach/WRCreature`, `WRBoss` | creatures, taming, mounts; bosses |
| `Source/Wildreach/WRCrafting`, `WRInventory`, `WRBuilding` | stations + Hearth, items + equipment, building + claims |
| `Source/Wildreach/WRLifeSkills`, `WRSocial` | life skills; clans, PvP rules, safe zones, death penalty |
| `Source/Wildreach/WRWorldDressing` | decoration, caves, biome hazards |
| `Source/Wildreach/WRMainMenu`, `WRPlayerMenus`, `WRHUD` | front end, in-game menus, HUD |
| `Source/Wildreach/Tests/` | automation tests |
| `Docs/` | technical plan, milestone plans, design review, terrain layout |

## Networking rules

- Server owns gameplay state. Clients send intent (input, RPCs), never results.
- Health/poise/buildup replicate to all; stamina/hunger/thirst/Heat/Chill owner-only.
- Anything that changes movement speed for the player goes through the CMC saved-move flags or runs the same
  deterministic check on the server and the owning client (ice friction).
- Server settings replicate through `AWRGameState`, so client-side rules (lock-on, PvP) match the host.
