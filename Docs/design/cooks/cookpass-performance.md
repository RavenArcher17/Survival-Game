# Cook pass: performance at Medium on an 8 GB GPU, the PSO cache, loading (cookpass-performance)

Lane: cookpass-performance (desktop, 2026-11-16). Repo E:\Dev\Wildreach (UE 5.8.3). Gates: compile only (Aaron tests).

## 0. Requirement ledger

| # | Requirement (from the brief / Aaron quotes) | Answered by (plan) | Proof (filled at the end) |
|---|---|---|---|
| R1 | Research how the best games handle performance on mid GPUs, PSO/shader stutter and loading; write the specific details used | 1 | this doc §1 |
| R2 | Plan every detail and gap in writing before building; list every build step | 3 | this doc §3 |
| R3 | Grade every built performance piece against THE BAR | 2 | this doc §2 |
| R4 | Medium on an 8 GB GPU: a first launch picks a sensible quality for this computer instead of Epic everywhere | 3.1 | |
| R5 | 8 GB VRAM: the texture streaming pool is sized to the card (no overflow into system memory, no needless blur) | 3.2 | |
| R6 | The player can see graphics memory in Video settings while choosing (8 GB players need to see it) | 3.3 | |
| R7 | A frame time readout Aaron can switch on to test Medium (fps, CPU, GPU, memory) | 3.4 | |
| R8 | PSO cache: each player's machine keeps the PSOs it missed and precompiles them next launch (Development builds too) | 3.5 | |
| R9 | PSO cache: the bundled (recorded) cache path is set up: stable keys written at cook, a record/expand tool, docs | 3.5 | |
| R10 | PSO: the runtime-built meshes (procedural meshes have no PSO precaching) compile their shaders behind the loading cover, not in play | 3.6 | |
| R11 | PSO: precompile fast while a cover/menu is up, in the background in play; precache boosted under the cover | 3.7 | |
| R12 | Loading: the cover holds until the island has streamed, the art is loaded and the shaders are ready (capped), and says which it is waiting for | 3.8 | |
| R13 | Loading/hitches: art the code loads by path (not just the data files) is preloaded too, from a generated list | 3.9 | |
| R14 | Wildreach is not a music game: no music on screen, no music-driven mechanic; remove any found | 3.10 | |
| R15 | Numbers in Data/tuning.json (data-driven like Raven's code) | all | |
| R16 | New settings rows work with mouse AND pad | 3.3, 3.4 | |
| R17 | Multiplayer: clients only; servers (dedicated, headless bots) untouched; nothing replicated changes | all | |
| R18 | CHANGELOG (Unreleased) + Docs/HANDOFF.md + Docs/PERFORMANCE.md in the same commit | 3.11 | |
| R19 | Compile clean, no new warnings | 4 | |
| R20 | Land in pieces, pull --rebase, add files by name, push; no release/tag | 4 | |
| R21 | LANDED line; out-of-scope finds go to LATER-COOKS | 4 | |

## 1. Research

### 1.1 Shader / PSO stutter (the "first time you see it, it hitches" problem)

- **Fortnite (UE5, Epic's own)**: ships a bundled PSO cache recorded from automated play, and on top of it 5.x
  *PSO precaching* (each component asks for the PSOs it could need when it loads; `r.PSOPrecache.ProxyCreationStrategy=1`
  delays drawing the object until they're ready, so the cost becomes "a prop appears a moment late" rather than a hitch).
  Epic's documentation is explicit that the two are complementary: precaching cannot see things it isn't told about
  (runtime-built geometry, global shaders used by code), the bundled cache covers those.
- **Call of Duty (MW2019 onward)**: "Shader optimization / Installing shaders" runs at the main menu with a percent,
  the player can browse menus (even play) while it runs, and it only has to happen once per driver. Lesson: compile
  behind screens the player is already looking at, show progress, never a silent long black.
- **Horizon Zero Dawn / Forbidden West PC, Hogwarts Legacy (patched), The Last of Us Part I**: first-launch shader
  build behind the start screen with a progress bar. TLOU Part I is the cautionary tale: 25-40 minutes blocking on
  8-core CPUs at launch (it compiled everything for every material). Lesson: compile what *this* scene needs, under a
  screen that's up anyway, cap the wait, keep the rest in the background.
- **Lies of P, Hi-Fi Rush (UE4)**: cited by Digital Foundry as stutter-free UE games: full PSO pre-compile during the
  first loading screen from a recorded cache, plus everything streamed in before a reveal. Lesson: a recorded cache is
  the only way to cover everything, and it needs a repeatable record step.
- **Star Wars Jedi: Survivor, Palworld (launch)**: UE traversal and shader stutter; Palworld's patch added a precompile
  at startup. Lesson for a survival open world like ours: the expensive moment is the first minute of a session (every
  material seen for the first time); move it under the loading cover.
- **Lyra (Epic's sample)**: `ULoadingScreenManager` keeps the loading screen up while
  `FShaderPipelineCache::NumPrecompilesRemaining() > 0`, and switches the pipeline cache's batch mode: Fast under the
  loading screen, Background in play. The engine header (ShaderPipelineCache.h) says the same.
- **Unity titles (Genshin, Ori)**: "shader warm-up": render every variant once on an off-screen/covered frame at load
  (ShaderVariantCollection.WarmUp). Lesson for our runtime meshes: draw each material once on a covered frame.
- **Engine facts checked in 5.8 source** (D:\UnrealEngine\UE_5.8):
  `r.ShaderPipelineCache.SaveUserCache` defaults on only in Shipping (`PIPELINE_CACHE_DEFAULT_ENABLED && UE_BUILD_SHIPPING`,
  PipelineFileCache.cpp:160): our packaged builds are Development, so missed PSOs are *not* remembered between launches
  today. `UProceduralMeshComponent` (62 of our source files build with it) has no PSO precache hook (no
  `CollectPSOPrecacheData` in the plugin): its first draw of each material compiles on the render thread, in play.
  `PipelineStateCache::NumActivePrecacheRequests()` and `PrecachePSOsBoostToHighestPriority(bool)` are exported (RHI).

### 1.2 Mid-range GPUs (8 GB) and quality defaults

- **8 GB failures** (TLOU Part I, Hogwarts Legacy, RE4 Remake at high textures): stutter comes from VRAM overflow
  (resources paged to system memory over PCIe), not from shading. The cure in every patched case was a texture budget
  sized to the card and a visible memory meter.
- **RE Engine (Resident Evil 2/3/4 remakes, Monster Hunter)**: the graphics menu shows a VRAM bar (used of the card's
  total) that turns red when the settings go over. **Doom Eternal**: a VRAM bar plus a texture pool size option, refuses
  settings that won't fit. **Forza Horizon 5**: a VRAM usage bar and a per-card default. Lesson: let the player see
  memory while choosing.
- **Fortnite, Lyra, Valorant**: run a hardware benchmark the first time the game starts and pick the scalability level
  (UGameUserSettings::RunHardwareBenchmark), so a 3060 Ti doesn't start on Epic at 1440p. Fortnite also caps texture
  quality by VRAM. Lesson: first launch = benchmark + VRAM cap, tell the player once what was chosen and where to change it.
- **Frame time readouts**: Fortnite ("Show FPS"), Apex, Doom Eternal (performance metrics with CPU/GPU ms), Cyberpunk.
  Testers need ms and which side is slow (CPU or GPU), not only fps.
- **UE scalability at Medium (5.8 BaseScalability.ini, checked)**: Lumen GI stays on at Medium (cheaper settings), Lumen
  reflections off (SSR), VSM 512 physical pages, `r.Streaming.PoolSize=600` with `LimitPoolSizeToVRAM`, MipBias 1. The
  pool is a fixed 600 MB whatever the card: too small for a 12 GB card, a large share on a 4 GB one.

### 1.3 Loading

- **Breath of the Wild / Tears of the Kingdom**: the loading screen tip and the world fading in only once the area
  around Link is ready: never a frame of missing ground. **Elden Ring**: the loading screen holds until assets near the
  grace are in; a short fade out. **Valheim / Enshrouded / Palworld** (survival peers): loading screen with a tip and a
  status line; Enshrouded shows "Compiling shaders" on the first launch.
- Our own cover (WRUIFlow) already waits for World Partition cells around the player (`IsIslandStreamed`), up to 15 s.
  It does not wait for the background preload (6 s of creature art) or shader compiles, so the first minute after the
  reveal is where hitches cluster.

## 2. THE BAR grade of what is built (performance)

| Piece | Where | Grade | What falls short |
|---|---|---|---|
| Wildlife cost by distance | WRSignificance.* | Good: measured, data-driven, server-only | nothing in this lane |
| Background preload | WRPreload.* + Data/preload.json | Partial | only data-file paths; ~120 material and ~50 mesh loads by literal path in code are still loaded on the spot; nobody waits for it |
| Loading cover | WRUIFlow.cpp:930-1090 | Partial | waits for cells only; status always "Loading the island…" |
| Video settings | WRUISettings.cpp:1715 | Partial | no memory shown; first launch is Epic everywhere until the player presses Auto-detect |
| PSO precaching | engine default | Partial | runtime meshes not covered; user cache off in Development; no bundled cache path |
| Texture pool | engine default | Weak for 8 GB | fixed 600 MB at Medium regardless of card |
| Frame readout | none (console `stat unit`) | Missing | |
| Music on screen | none found | Pass | WRAudioDirector only switches the soundtrack on beat/bar lines; no UI shows music (grep of every WRUI*/HUD file) |

## 3. Plan

All new code is client-side (`FApp::CanEverRender()` and not a dedicated server). New files: `Source/Wildreach/WRPerf.h/.cpp`
(prefix `WRPerf`, unity-safe helpers in `namespace WRPerfDetail`). Tuning block `perf` in Data/tuning.json.

### 3.1 First launch picks the quality (R4)
- `UWRPerfSubsystem` (GameInstance subsystem). On the first frame the main menu is open (so test tools that go straight
  to a map never trigger it; also skipped with `-wrbot`, `-unattended`, `-wrnoautoperf`), if `[Wildreach.Perf] Picked`
  isn't in GameUserSettings: `RunHardwareBenchmark` + `ApplyHardwareBenchmarkResults`, then the VRAM caps
  (`perf.vramTiers`: below 4.5 GB textures Low, shadows/GI Medium at most; below 6.5 GB textures Medium; below 8.5 GB
  textures High), `ApplySettings`, save `Picked=1`.
- One toast through the flow ("Graphics set for this computer: Medium · 8 GB graphics card. Settings › Video to
  change.") the first time a game world shows after the pick.
- Auto-detect on the Video page uses the same function (benchmark + VRAM caps), so the button and first launch agree.

### 3.2 Texture pool sized to the card (R5)
- On start and whenever texture quality changes (checked twice a second), `r.Streaming.PoolSize` is set by code to
  `clamp(VRAM MB x perf.texturePool.share[level], min[level], max[level])`: shares 0.06/0.09/0.12/0.15, mins
  300/400/500/600, maxes 600/1200/2000/3000. An 8 GB card: Low 491, Medium 737 (engine 600), High 983, Epic 1228 MB;
  a 4 GB card at Medium 400 (engine 600: over a tenth of the card). `LimitPoolSizeToVRAM` stays on.
- VRAM comes from DXGI (`IDXGIAdapter3::QueryVideoMemoryInfo`, local segment, the adapter matching `GRHIAdapterName`):
  `Budget` (what the OS gives this process) and `CurrentUsage`; fallback `RHIGetTextureMemoryStats().DedicatedVideoMemory`.

### 3.3 Graphics memory on the Video page (R6, R16)
- A row under the Quality list: "Graphics memory" with a bar (brass fill, warn colour from 85 %, danger from 95 %) and
  "5.2 of 7.6 GB in use" (live, twice a second), sub line "This card's budget; keep it under the line for smooth play".
  Display-only (not focusable), so pad navigation is unchanged. Reads `WRPerf::Memory()`.

### 3.4 Frame time readout (R7, R16)
- Video page switch "Show frame time" (`SWRSwitch`, mouse and pad like every switch), sub "Frame rate, CPU and GPU time,
  graphics memory". Saved `[Wildreach.Perf] ShowFrameTime`. Also console `wr.perfoverlay`.
- Drawn by the perf subsystem as a viewport widget (z 70, above the HUD, under nothing interactive), top right under
  the compass row's height: one line "118 fps  8.4 ms · CPU 3.4 · Draw 8.2 · GPU 7.6 · 5.2 / 7.6 GB", Body 12, ink on
  a glass chip; the slowest of CPU/Draw/GPU in brass so the bottleneck reads at a glance. Values averaged over 0.5 s.

### 3.5 PSO caches (R8, R9)
- `Config/DefaultEngine.ini` `[ConsoleVariables]`: `r.ShaderPipelineCache.SaveUserCache=1` (remember missed PSOs in
  Development too), `r.ShaderPipelineCache.Enabled=1`. `[DevOptions.Shaders] NeedsShaderStableKeys=true` (the cook writes
  stable keys so a recorded cache can be expanded).
- `Tools/perf/pso_cache.ps1`: `-Collect <dir>` copies `*.rec.upipelinecache` from a packaged run's Saved/CollectedPSOs;
  `-Expand` runs `ShaderPipelineCacheTools expand` against the cook's `.shk` files into
  `Build/Windows/PipelineCaches/Wildreach_PCD3D_SM6.spc` (and SM5), engine path parameter (defaults D:, falls back C:).
  Not run in this lane (needs a package and a play session: Aaron's / a ship lane's step). Documented in PERFORMANCE.md.

### 3.6 Warm-up of runtime-mesh materials (R10)
- `UWRPerfWarmup` (world subsystem, clients). When the world's preload has finished, every loaded surface material in
  it (`UMaterialInterface`, domain Surface) gets one small procedural box on an actor that follows the camera 2.5 m
  ahead, in a 12-wide grid of 18 cm boxes, casting shadows and moving (so depth, base, shadow and velocity passes all
  draw). Only while the loading cover is black. Held `perf.warmup.frames` (6) rendered frames, then destroyed. Logged
  ("PERF warm-up: N materials in M ms").
- Edge cases: cover released early (timeout) → warm-up still finishes (2.5 m ahead for 6 frames is a visible flash of
  boxes, so it is skipped and logged instead if the cover is already gone); no camera yet → wait; world torn down →
  actor goes with it.

### 3.7 Batching (R11)
- Perf subsystem tick: covered (flow cover black > 0.5) or a menu up → `FShaderPipelineCache::SetBatchMode(Fast)` and
  `PrecachePSOsBoostToHighestPriority(true)`; in play → `Background` and `false`. Changes only on transitions.

### 3.8 The cover waits for art and shaders (R12)
- `FWRFlow`: the island check becomes "island streamed AND `WRPerf::Ready(World)`" where Ready = preload done, warm-up
  done, `NumActivePrecacheRequests() + NumPrecompilesRemaining() == 0`. Timeout: still `flow.streamMaxSeconds` (15) for
  the cells; while only shaders/art remain the wait may run to `perf.readyMaxSeconds` (30). Logged with the reason.
- The status line and the loading screen's line read `WRPerf::Stage`: "Loading the island…" / "Unpacking the art…" /
  "Preparing shaders for this computer… 214" (count). The movie player's own screen keeps its fixed line (it draws on
  another thread).

### 3.9 Code-loaded art preloaded (R13)
- `Tools/perf/scan_code_assets.mjs` scans Source/Wildreach (*.cpp/*.h, not Tests) for `TEXT("/Game/...")` literals
  without `%`, drops /Game/Maps, writes `Data/preload_code.json` (`paths`, sorted, with a comment). preload.json's
  `scan` gains it. Re-run after adding a path (documented in preload.json's comment).

### 3.10 Music (R14)
- Audited: no music UI anywhere (grep of WRUI*.cpp, WRHUD*, WRPlayerUI*: the only "beat" is the breath ring's pulse at
  1.6 Hz, a heartbeat, not the music); WRAudioDirector waits for a beat/bar only to switch pieces smoothly. Nothing to
  remove. Recorded with file:line in §5.

### 3.11 Docs (R18)
- CHANGELOG "Unreleased: performance cook pass" section; HANDOFF line; PERFORMANCE.md section "Cook pass 11-16".

### Build steps
1. tuning `perf` block; DefaultEngine.ini lines; Tools/perf/scan_code_assets.mjs + run it → Data/preload_code.json; preload.json scan.
2. WRPerf.h/.cpp: memory (DXGI), pool sizing, first-run pick, batching, overlay widget, Ready/Stage, warm-up subsystem.
3. WRPreload: `IsDone()`, loaded-assets accessor.
4. WRUIFlow: Ready in the wait, Stage text, NoteCovered, the first-run toast.
5. WRUISettings: memory row, frame-time switch, Auto-detect → WRPerf::AutoPick.
6. Build.cs: dxgi.lib on Windows.
7. Compile, fix, docs, commit, push (two commits: config/data/tools, then code+docs).
8. Spot-check against this plan.

## 4. Gates
Compile only (Build.bat WildreachEditor, D: engine). No tests run (Aaron tests).

## 5. Proof (filled after building)

Landed in Wildreach a7836e2 (one commit: compiled clean, also with adaptive unity off). Line numbers at a7836e2.

| # | Done | Where |
|---|---|---|
| R1 | yes | §1 of this doc |
| R2 | yes | §3 (written before any code) |
| R3 | yes | §2 |
| R4 | yes | WRPerf.cpp:354 `AutoPick` (benchmark, `perf.vramTiers` caps, quality levels only), :545 `TickFirstRun` (first menu, untouched-Epic only, never bots/-unattended/editor), toast WRUIFlow.cpp:1335, Auto-detect button WRUISettings.cpp:2663 |
| R5 | yes | WRPerf.cpp:519 `TickPool` (`perf.texturePool`, SetByCode, re-applied on texture level change, not in the editor process) |
| R6 | yes | WRUISettings.cpp:1772 row, :1843 `MemoryMeter` (DXGI use / budget, WRPerf.cpp:306; brass / amber 85% / red 95%, the warn mark) |
| R7 | yes | WRPerf.cpp:166 `SWRPerfOverlay` (fps, frame ms, CPU / Draw / GPU with the slowest in brass, memory, shaders pending), :267 `wr.perfoverlay`, :566 re-attached after map changes; switch WRUISettings.cpp:1827 |
| R8 | yes | Config/DefaultEngine.ini:75 `r.ShaderPipelineCache.SaveUserCache=1` |
| R9 | yes (tooling; not recorded) | DefaultEngine.ini:80 `NeedsShaderStableKeys=true`; Tools/perf/pso_cache.ps1; .gitignore:9; Docs/PERFORMANCE.md "PSO cache" |
| R10 | yes | WRPerf.cpp:609 / :648 `UWRPerfWarmup` (one procedural mesh, a section per preloaded surface material, grid fitted to the FOV, moved with the camera, shadows on, `perf.warmup.frames`, only under the black cover) |
| R11 | yes | WRPerf.cpp:444 `NoteCovered` (Fast + boost under cover, Background in play), called WRUIFlow.cpp:1123 |
| R12 | yes | WRUIFlow.cpp:1089-1100 (`WRPerf::Ready`, `readyMaxSeconds` first wait / `rewaitMaxSeconds` later), WRPerf.cpp:420 / :428 `Ready` / `Stage`; the line WRUIFlow.cpp:295-296 and the waking line |
| R13 | yes | Tools/perf/scan_code_assets.mjs -> Data/preload_code.json (44 paths), Data/preload.json scan |
| R14 | yes (nothing to remove) | audit: no music UI in WRUI*/HUD/PlayerUI; WRUIHud.cpp:1020 is the breath ring's 1.6 Hz pulse; WRAudioDirector.cpp:872 switches pieces on a beat/bar only |
| R15 | yes | Data/tuning.json:659 `perf` block (vramTiers, texturePool, readyMaxSeconds, rewaitMaxSeconds, warmup, overlay) |
| R16 | yes | the switch is `SWRSwitch` (same focus/pad path as every switch); the meter is display-only |
| R17 | yes | every piece gated on `FApp::CanEverRender()` / not dedicated server (WRPerf.cpp:484, :591); nothing replicated |
| R18 | yes | CHANGELOG "Unreleased: the performance cook pass", Docs/HANDOFF.md (Where the game stands + Releasing step 4), Docs/PERFORMANCE.md "Cook pass 2026-11-16" |
| R19 | yes | Build.bat WildreachEditor: Succeeded, no warnings (adaptive and full unity) |
| R20 | yes | a7836e2 pushed to master (pulled first, files by name) |
| R21 | yes | LANDED + LATER-COOKS lines |

Changed from the plan while building: the frame time readout sits at the top-left edge (the clock plate holds the top
right at 20 px); the warm-up grid is fitted to the camera's FOV (a fixed 12-wide grid would put some boxes off screen,
where they'd be culled and never compile); deaths and long jumps hold the cover for at most `perf.rewaitMaxSeconds` (4)
once the cells are in, so background compiles in a running world never hold a respawn for 30 s.

Not finished: the bundled PSO cache is not recorded (needs a package and a play session with -logPSO); nothing measured
on an 8 GB card (Tools\benchmark.ps1 Medium/Epic before and after).
