# Wildreach (working title)

Multiplayer open-world survival, first + third person, UE 5.8, C++.

## Build

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" WildreachEditor Win64 Development -Project="C:\Dev\Wildreach\Wildreach.uproject" -WaitMutex
```

Or double-click `Wildreach.uproject` (it offers to build if binaries are missing).

## Play / test multiplayer

```
Tools\play.ps1 local -Clients 2   # dedicated server + 2 windowed clients
Tools\play.ps1 listen             # host-and-play
Tools\play.ps1 server             # dedicated server only (port 7777)
Tools\play.ps1 client             # join 127.0.0.1:7777
```

These run from the editor binaries, so no packaging is needed. A *packaged* dedicated
server needs a source build of the engine (free from Epic's GitHub); that comes later.

Controls: WASD, mouse, Space jump, Shift sprint, V first/third person.

## Layout

| Path | What |
|---|---|
| `Source/Wildreach/WRCharacter` | Survivor pawn: FP/TP cameras, code-built Enhanced Input, death/respawn |
| `Source/Wildreach/WRCharacterMovementComponent` | Client-predicted sprint (saved-move flag) |
| `Source/Wildreach/WRSurvivalComponent` | Server-authoritative health/hunger/thirst/stamina |
| `Source/Wildreach/WRGameMode`, `WRHUD` | Respawn timer, debug canvas HUD |
| `Content/Maps/L_Wildreach` | World Partition test map, 4x4 km grid plane |
| `Tools/make_test_map.py` | Rebuilds the test map headless (`Tools\make_map.ps1`) |

## Networking rules

- Server owns gameplay state. Clients send intent (input, RPCs), never results.
- Health replicates to all; hunger/thirst/stamina owner-only.
- Anything movement-related goes through the CMC saved-move path, not RPCs.
