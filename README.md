# Wildreach (working title)

Multiplayer open-world survival, first + third person, UE 5.8, C++. The first playable (fire + frost, Volcanic +
Frozen tundra) runs on placeholder art. See `CHANGELOG.md` for what's built and its known gaps.

## Play with friends

```
Tools\package.ps1          # -> C:\Dev\WildreachBuild\Windows\Wildreach.exe (+ README.txt for players)
Tools\make_installer.ps1   # -> C:\Dev\WildreachBuild\Wildreach-Setup.exe (one file to send friends)
```

Players download `Wildreach-Setup.exe` from the latest GitHub release
(https://github.com/RavenArcher17/Survival-Game/releases/latest/download/Wildreach-Setup.exe, ~1.6 GB). Double-clicking
it installs to the player's own folder (no admin), adds Desktop and Start Menu shortcuts, installs the VC++ runtime only
if it's missing, and starts the game; a newer Setup updates in place and keeps saves. The shortcuts start
`WildreachUpdater.exe`, which installs any newer GitHub release by itself before starting the game (the main menu
offers **Update now** too), so players only download Setup once. It isn't code-signed, so Windows may show "Windows
protected your PC": More info, then Run anyway. Everyone runs the game, types a name, picks a
Male or Female body and Fire or Frost. One person clicks **Host** (their PC is the server; over the internet they forward UDP 7777
to their PC). Everyone else types the host's IP and clicks **Join**. Saves live on the host
(`Wildreach\Saved\ServerDB`). F10 goes back to the menu.

A packaged **dedicated server** needs Unreal built from source: `Tools\build_engine.ps1` (several hours,
~200 GB; log `C:\Dev\UE5Source-build.log`). Until then the host plays and hosts at once.

**Releasing an update:** bump `ProjectVersion` in `Config/DefaultGame.ini` (e.g. 0.2.1), run `Tools\package.ps1` and
`Tools\make_installer.ps1`, then publish a GitHub release tagged `v0.2.1` with `Wildreach-Setup.exe` attached
(`gh release create v0.2.1 C:\Dev\WildreachBuild\Wildreach-Setup.exe --target master --latest`). Installed copies
update on their next start. Clients and hosts must be on the same version to play together.

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
`node Tools/terrain/dressing.mjs` (writes `Data/island_dressing.json`). Move or re-balance the wildlife without a rebuild:
`node Tools/terrain/spawns.mjs` (writes `Data/island_spawns.json`: 81 packs, every wild species in its biome; a pack
comes out when a player is within 250 m, `wildlife` in `tuning.json`).

### Controls

Every key below is a default: **Esc → Controls** rebinds any of them (saved in `GameUserSettings.ini`,
`[Wildreach.Keys]`), and **H** shows the current ones in game.

| Key | Action |
|---|---|
| WASD / mouse | move / look |
| LMB / R | light / heavy attack |
| RMB (hold) | block; the first moment of a block is a parry |
| C | dodge roll (i-frames) |
| 1–8 / mouse wheel | item hotbar: equip or use what's on the key (drag items onto it in the inventory); the wheel steps along it |
| Q / Z (hold) | ability wheel 1 / 2: pick an ability with the mouse, release to keep it; a tap casts the picked one. Esc → Controls can give each ability its own key instead (or both) |
| Tab | lock on |
| E | interact: stations, chests, doors, beds, crops, pens, Hearth, claim totem, loot bags; on a knocked-out creature, the Taming panel; hold E on your tame for its order wheel |
| I / J / P / K / M | the journal: inventory / crafting / paths (learn nodes, fill the wheels) / life skills (perks) / map |
| B | build bar: click a piece, then LMB place, R rotate (45° with snap off), F snap on/off, RMB cancel, B leave; the ghost is red with the reason when it won't go. Demolish from the bar: aim at your piece, LMB removes it for half back |
| LMB at a tree / rock / shrub | gather wood / stone / fiber (a hatchet or pick in your bag gathers more) |
| G | swap sword-and-shield for the ranged weapon (bow, crossbow, blowgun, spears, bolas) and back |
| with it out: RMB (hold) / LMB / wheel | aim over the shoulder / fire or throw at the crosshair / next ammo (stone, tranq) |
| Shift / Space / V | sprint / jump / first-third person |
| T | chat |
| Esc | close what's open; with nothing open, the game menu (Controls, Video, main menu, quit) |
| F10 | back to the main menu |

### Dev commands (console)
`WRGivePoints fire 30`, `WRAutoSpend fire 30`, `WRLearn fire fire_wall`, `WRCast 3`, `WRSpawn cinderfang 3`,
`WRSpawn vulkarn 1`, `WRBossDmgPct 30 frost`, `WRGive iron_ore 20`, `WREquip copper_sword`, `WRWeapon iron fine 6`,
`WRSkill mining 50`, `WRTameStat`, `WRTorpor 100`, `WRTameGive kibble`, `WRNarcotic`, `WRClan create Name`, `WRHazard`, `WRHeal`, `WRTeleport x y`,
`WRGather tree` (bot gathers: tree / rock / shrub / node / any / off), `WRHour 21` (jump the clock), `WRSpawnAt cinderfang X Y`, `WRCreatureHP rimeclaw 0.3`, `WRTameAll stay`, `WRSky` (log the sky), `WRWildlife` (packs out, what's near),
`WRGive hunting_bow 1`, `WRGive tranq_arrow 20`, `WRRanged 1`, `WRHold 30` (root nearby creatures), `WRClearCreatures`, `WRMenu inventory 3` (open a menu on a line: inventory / build / skills / paths / map / confirm / none),
`WR.ReloadTuning` (re-read `Data/tuning.json` live). Tests can override numbers: `-wrtune=key:value,key:value`.
UI (screenshots and tests): `WRWheel 0 2` (hold a wheel open on a wedge; 2 = tame orders, -1 closes), `WRHelp`,
`WRMenu crafting|settings|hearth 0`, `WRSettings video|controls|listen|key G|cancel`, `WRMainMenu [settings]`,
`WRUISelect ranged`, `WRUIMouse click|rclick|drag ...` (real Slate input), `WRCmd <server command>`,
`WRFlow preview death|loading|island|levelup|point|recipe|discover <place>|boss <id>|tame <id>|hint <id>` (the moments between play),
`WRFlow forget` / `hints on|off` / `quiet on|off` (this player's discoveries and hints; moments off for other screenshots), `WRFlow shotondeath 3`,
`WRFlow near tree`, `WRFlow cam X Y Z Yaw Pitch` (try a view, e.g. for the menu backdrop). `-oldui` keeps the
old text HUD.

## Test

`Tools\alltests.ps1` builds and runs everything below in turn (about 40 minutes) and prints a PASS / FAIL table
(`-SkipBuild`, `-Only ranged,tame`). Each bot test ends with its checks (`PASS` / `FAIL` lines) and exits 1 on a failure.
Arena tests give their server `-wrworld=<name>`, so each keeps its own world save (`Saved/ServerDB/world_<name>.json`)
and they can run side by side.

```
Tools\test.ps1          # automation tests (rules, combat scenarios, ranged items, taming food, creature behaviour, clock, wildlife sites, versions)
Tools\mptest.ps1        # headless server + bots, PvE (-Scenario pvp for duels)
Tools\persisttest.ps1   # save -> server restart -> load
Tools\crafttest.ps1     # smelt, craft, infuse, upgrade, Hearth tiers, sockets, every ranged weapon + ammo, bow wear + repair
Tools\buildtest.ps1     # claims, code locks, raids, collapse, world save
Tools\housetest.ps1     # a two-room, two-storey house through the build ghost: snapping, refusals, stairs, doors, roof
Tools\survivaltest.ps1  # fresh player on the island: gather bare-handed, crude tools, hut + workbench, stone tools
Tools\skytest.ps1       # day and night: sun, moon and exposure at noon / sunset / midnight, clock rate, time kept over a restart (-Shots, -Arena)
Tools\tametest.ps1      # offer food, ride, tranq knockout + effectiveness + narcotic + kibble, eggs, orders, waking loses progress, tames saved (-Shots)
Tools\aitest.ps1        # creature AI: a pack hunt, sleep by day / hunt at night, a hurt creature fleeing, charge / stomp / spit / lunge (-Shots)
Tools\rangedtest.ps1    # every ranged weapon at a rooted Cinderfang: hits, tranq knockout, bola, spear pickup (-Body f)
Tools\wildlifetest.ps1  # island wildlife: a herd comes out when a player arrives (each side), goes quiet, comes back (-Shots)
Tools\updatertest.ps1   # auto-update end to end with fake releases and a local GitHub stand-in (no Unreal needed)
Tools\versiontest.ps1   # another version can't join and the menu says why; the menu offers a newer release (local feed)
Tools\bosstest.ps1      # all six bosses: phases, counterplay, party scaling, 10% loot rule
Tools\socialtest.ps1    # PvP preset, clans, Souls-like death bags, life skills + perks
Tools\flowtest.ps1      # death by creatures: death note, death screen, back after the countdown at your camp; a bed respawn; a hint remembered
Tools\spelltest.ps1     # Paths menu learn + the four tree spells
Tools\terrain\walktest.ps1 / hazardtest.ps1 / island_shots.ps1   # island traversal, lava + ice, screenshots
Tools\uishots.ps1       # a screenshot of every stone-and-leather screen (HUD, wheels, journal tabs, build, storage, Hearth, claim, taming, settings, then the moments between play: death, loading, banners, hints; -MainMenu: the front end; -Flow: just the moments)
Tools\menushots.ps1     # the same for the old text menus (-oldui)
```

## Layout

| Path | What |
|---|---|
| `Data/tuning.json` | every gameplay number (starting numbers + placeholders) |
| `Data/server_presets.json` | server settings presets |
| `Data/paths/*.json`, `lifeskills.json` | skill trees, life skills |
| `Data/creatures.json`, `items.json`, `recipes.json`, `pieces.json` | roster + bosses, items, recipes, building pieces |
| `Data/island_dressing.json` | trees, rocks, caves, lava/ice cells (generated) |
| `Data/island_spawns.json` | wildlife sites: creature, place, pack size, respawn (generated) |
| `Tools/installer/` | `WildreachSetup.cs` (the one-file installer), `WildreachUpdater.cs` (the shortcut: auto-update) |
| `Source/Wildreach/WRCombatLibrary` | the damage pipeline (block/parry/statuses/combos) |
| `Source/Wildreach/WRAbilities` | all GAS abilities |
| `Source/Wildreach/WRPathTypes`, `WRPathComponent` | trees, rules, progress, grants |
| `Source/Wildreach/WRCreature`, `WRBoss` | creatures, taming, mounts; bosses |
| `Source/Wildreach/WRCrafting`, `WRInventory`, `WRBuilding` | stations + Hearth, items + equipment, building + claims |
| `Source/Wildreach/WRLifeSkills`, `WRSocial` | life skills; clans, PvP rules, safe zones, death penalty |
| `Source/Wildreach/WRWorldDressing` | decoration, caves, biome hazards |
| `Source/Wildreach/WRMainMenu`, `WRPlayerMenus`, `WRHUD` | front end, in-game menu commands, world-space bars and damage numbers |
| `Source/Wildreach/WRUIFlow`, `WRFlowDeath`, `WRUIMotion` | the moments between play: death screen + fades, loading screens, banners, toasts, hints; UI motion |
| `Source/Wildreach/WRPlayerUI`, `WRUI*` | the stone-and-leather Slate UI: style + widget kit (`WRUIStyle`, `WRUIWidgets`), key bindings (`WRKeyBindings`), HUD, wheels, journal tabs, screens (each registers itself by name) |
| `Source/Wildreach/Tests/` | automation tests |
| `Docs/` | technical plan, milestone plans, design review, terrain layout |

## Networking rules

- Server owns gameplay state. Clients send intent (input, RPCs), never results.
- Health/poise/buildup replicate to all; stamina/hunger/thirst/Heat/Chill owner-only.
- Anything that changes movement speed for the player goes through the CMC saved-move flags or runs the same
  deterministic check on the server and the owning client (ice friction).
- Server settings replicate through `AWRGameState`, so client-side rules (lock-on, PvP) match the host.
