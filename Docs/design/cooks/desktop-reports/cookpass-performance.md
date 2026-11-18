# cookpass-performance: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-performance.out).

LANDED cookpass-performance a7836e2 (Wildreach), 8027c4d (showcase-notes research doc)

The performance pass is built and pushed. It compiles with no errors or warnings, including a full non-adaptive unity build. Nothing was run or measured, per the gates.

**What changed**
- **First launch picks the graphics quality.** Before, the game started on Epic everywhere. Now the first time the main menu opens, it runs the hardware benchmark, then caps texture quality by graphics card memory (an 8 GB card gets textures at High at most). It changes quality levels only, not the window or resolution, and a one-time toast says what was picked and where to change it. Players who had already changed their levels keep them, and test bots, `-unattended` tools and the editor never trigger it. The Auto-detect button now does the same thing.
- **Texture memory is sized to the card.** It's now a share of the card's memory per texture level instead of the engine's fixed 600 MB at Medium. An 8 GB card gets 737 MB at Medium.
- **Video settings show graphics memory.** A meter reads e.g. "5.2 of 7.6 GB" and turns amber at 85% and red at 95%.
- **"Show frame time" switch** (also `wr.perfoverlay`): a small strip at the top-left edge with fps, CPU, draw and GPU times (the slowest in brass), graphics memory, and how many shaders are still compiling.
- **Shaders compile behind the loading screen instead of during play.**
  - Every material the background preload loads is drawn once on small boxes while the screen is black. Runtime-built meshes, used in 62 source files, get no shader precaching from the engine, so this is the only thing preparing them.
  - Packaged Development builds now remember shaders compiled during a session and prepare them at the next launch. The engine only does this in Shipping builds by default.
  - Compiling runs at full speed while the screen is black and in the background during play.
- **The loading screen waits for the art and shaders, not just the terrain.** It holds up to 30 s on joining and up to 4 s after a death or teleport, and its line says what it's waiting for ("Preparing shaders for this computer… 214"). The main menu stays usable while this runs.
- **Art the C++ loads by fixed path is now preloaded too.** A scan tool lists 44 such paths in `Data/preload_code.json`.
- **Bundled shader cache: set up but not recorded.** The cook now writes the keys it needs, `Tools/perf/pso_cache.ps1` turns a recording into the cache file, and `.gitignore` lets that file through.

**Requirement ledger** (each row has file:line proof in the research doc)

| # | Requirement | Status | Where |
|---|---|---|---|
| R1 | Research | done | cooks/cookpass-performance.md §1 |
| R2 | Plan before building | done | same doc, §3 |
| R3 | Grade the built pieces | done | same doc, §2 |
| R4 | First launch picks quality | done | `WRPerf.cpp:354, :545`; toast `WRUIFlow.cpp:1335`; Auto-detect button `WRUISettings.cpp:2663` |
| R5 | Texture memory sized to the card | done | `WRPerf.cpp:519` |
| R6 | Memory meter on the Video page | done | `WRUISettings.cpp:1772, 1843`; `WRPerf.cpp:306` |
| R7 | Frame time readout | done | `WRPerf.cpp:166, 267, 566`; `WRUISettings.cpp:1827` |
| R8 | Per-machine shader cache | done | `Config/DefaultEngine.ini:75` |
| R9 | Bundled shader cache path | tooling done, not recorded | `DefaultEngine.ini:80`; `Tools/perf/pso_cache.ps1`; `.gitignore:9` |
| R10 | Warm-up for runtime-built meshes | done | `WRPerf.cpp:609, 648` |
| R11 | Fast compile under the cover, background in play | done | `WRPerf.cpp:444`; `WRUIFlow.cpp:1123` |
| R12 | Loading screen waits for art and shaders, says which | done | `WRUIFlow.cpp:1089-1100, 295-296`; `WRPerf.cpp:420, 428` |
| R13 | Art loaded by path in code is preloaded | done | `Tools/perf/scan_code_assets.mjs`; `Data/preload_code.json` |
| R14 | No music on screen or music-driven mechanic | done, nothing to remove | The only pulse on screen is the breath ring (`WRUIHud.cpp:1020`). The music only switches pieces on a beat or bar line (`WRAudioDirector.cpp:872`). |
| R15 | Numbers in tuning.json | done | `Data/tuning.json:659` (`perf` block) |
| R16 | Mouse and pad | done | The switch uses the same `SWRSwitch` as every other switch. The meter is display-only, so the pad skips it. |
| R17 | Clients only | done | Checks on rendering and dedicated server, `WRPerf.cpp:484, 591` |
| R18 | CHANGELOG, HANDOFF, PERFORMANCE.md | done | a7836e2 |
| R19 | Compiles clean | done | adaptive and full unity |
| R20 | Pull first, add by name, push | done | a7836e2 |
| R21 | LANDED and LATER-COOKS lines | done | both files on the handoff share |

**Not finished**
- **Bundled shader cache:** it needs a packaged build played once with `-logPSO`; the route is in the script's header.
- **Not measured on an 8 GB card.** `Tools\benchmark.ps1` before and after is the check. That script hardcodes the C: engine path, so it needs the D: path on this PC.
- **Longer first loading screen:** the first join after launch now shows the loading screen a few seconds longer, while the preload (about 6 s) and shader compiles finish. That is intended, but please check how it feels.

New binary files: none (0 bytes).
EXIT 0
