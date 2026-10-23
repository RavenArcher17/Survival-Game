# Wildreach (working title)

Multiplayer open-world survival, first + third person, UE 5.8, C++. See `CHANGELOG.md` for what's built.

## Build

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" WildreachEditor Win64 Development -Project="C:\Dev\Wildreach\Wildreach.uproject" -WaitMutex
```

Or double-click `Wildreach.uproject` (it offers to build if binaries are missing).

## Play

```
Tools\play.ps1 local -Clients 2   # dedicated server + 2 windowed clients
Tools\play.ps1 listen             # host-and-play
Tools\play.ps1 server             # dedicated server only (port 7777)
Tools\play.ps1 client             # join 127.0.0.1:7777
```

Combat testing happens on `/Game/Maps/L_CombatArena` (dummies, a Cinderfang pack west, a Gloomhowl pack east,
both altars). Join with `?Path=fire` or `?Path=frost` to pick your main path, `?WRName=You` to keep your save.

### Controls

| Key | Action |
|---|---|
| WASD / mouse | move / look |
| LMB / R | light / heavy attack |
| RMB (hold) | block; the first moment of a block is a parry |
| C | dodge roll (i-frames) |
| 1–8 | path abilities in your quick slots |
| Tab | lock on |
| E | interact (boss altars) |
| Shift / Space / V | sprint / jump / first-third person |

### Dev commands (console)
`WRGivePoints fire 30`, `WRAutoSpend fire 30`, `WRSetSub frost`, `WRBuy fire fire_wall`, `WRSpawn cinderfang 3`,
`WRStatus frozen`, `WRWeapon iron fine 6`, `WRHeal`, `WRGoToAltar frost`, `WR.ReloadTuning` (re-read `Data/tuning.json` live).

## Test

```
Tools\test.ps1          # automation tests (rules + deterministic combat scenarios)
Tools\mptest.ps1        # headless server + bots, PvE (-Scenario pvp for duels)
Tools\persisttest.ps1   # save -> server restart -> load
Tools\shots.ps1         # rendered client screenshots -> Saved\Screenshots
```

## Layout

| Path | What |
|---|---|
| `Data/tuning.json` | every gameplay number (starting numbers + placeholders) |
| `Data/paths/*.json` | skill trees (generated from the design doc) |
| `Data/creatures.json` | creature roster |
| `Source/Wildreach/WRCombatLibrary` | the damage pipeline (block/parry/statuses/combos) |
| `Source/Wildreach/WRStatusComponent` | status timers, buildup, regen, hunger/thirst |
| `Source/Wildreach/WRAbilities` | all GAS abilities |
| `Source/Wildreach/WRPathTypes`, `WRPathComponent` | trees, rules, progress, grants |
| `Source/Wildreach/WRCreature` | creatures, dummy, AI, spawner |
| `Source/Wildreach/Tests/` | automation tests |
| `Docs/` | technical plan, milestone plans, design review |

## Networking rules

- Server owns gameplay state. Clients send intent (input, RPCs), never results.
- Health/poise/buildup replicate to all; stamina/hunger/thirst/Heat/Chill owner-only.
- Anything that changes movement speed for the player goes through the CMC saved-move flags, not RPCs.
