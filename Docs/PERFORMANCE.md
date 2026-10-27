# Performance

Frame-time numbers for Wildreach, from `Tools\benchmark.ps1` (see its header for the route and options). Run it before
and after anything that could cost frames (animation, sound, effects, world detail, settings) and compare the rows.

## How the benchmark runs

- **Client runs**: a dedicated server and a rendered client at 1920x1080 windowed, frame rate uncapped, VSync off, sound
  on, the editor binary in `-game` (uncooked, so a little slower than a packaged build). Bot commands walk a fixed route,
  the walk test's WalkA: **camp** = 20 s looking around the north camp (WalkA's start, -100400 -129600), **walk** = 90 s
  east along WalkA's line toward the north pass (~400 m, the packs on the way come out, a Basaltback herd passes close),
  **move** = a 2.5 km teleport to the tundra's east camp and 25 s there (terrain and props streaming in). The client's
  CSV profiler (`csvprofile start` / `stop`) records every frame. Epic and Medium (`scalability 3` / `1`), each twice,
  interleaved, a fresh server and world each time.
- **Server runs**: the server with a headless player walking the same stretch (camp, then 60 s of the walk), once with
  the usual wildlife (a pack comes out within 250 m of a player, plus the always-on packs and cave colonies) and once with
  every island pack out (`wildlife.activationRadius` past the island: all 169 creatures, as on a server with players all
  over the island). It captures 5400 frames from boot and is cut to the time the player walked.
- **Machine load**: other agents' builds, editors and renders share this machine (i7-13700K, 24 threads, 32 GB, RTX
  4070 Ti). Each row says what else ran: CPU used by other programs during the run, GPU used by other programs (Windows'
  per-process GPU Engine counters, busiest named) next to this run's own share, and the other Unreal / game /
  shader-compiler / build / Python processes. Rendered runs take the same machine-wide lock as `Tools\uishots.ps1`.
  Numbers from a loaded machine are worse than the game's; compare rows taken under similar load and prefer the best
  consistent pair.

## What the columns mean

- **avg (fps), p95, p99**: frame time in ms over all frames (p95 = 95% of frames were at least this fast).
- **game / draw / GPU**: game thread, render thread and GPU time per frame (ms, averages). The slowest of the three
  sets the frame rate.
- **hitches**: frames over 50 ms / over 100 ms (visible stutters). **worst**: the longest frame.
- **GC**: the longest garbage-collection pause (ms) and how many frames ran GC.
- **memory**: the client's physical memory (peak, from the CSV).
- **phases**: average / p99 frame time in each part of the route; "streaming done" = seconds after the long move until
  nothing was still loading or becoming visible.
- **Max-iter**: `GetSimulationTimeStep() - Max iterations` warnings in the log during the run.
- **Server work**: the game thread's work per frame (sum of its exclusive CSV timings; a dedicated server sleeps the rest
  of each 33 ms frame at 30 Hz), the biggest categories (ms), and ticks per frame.

## Baseline (9c707cb, before the polish pass), what it showed

Read the rows taken on a quiet machine (low "GPU others"); the others show what other agents' renders and the local AI
mesh pipeline (python, up to 58% of the GPU) did to the numbers, not the game.

- **Client frame**: Epic about 8.5 ms (118 fps), p99 about 12 ms; the render thread is the critical path (draw is about
  the whole frame), GPU 7.5 ms at Epic and 4 ms at Medium, game thread 3 ms (1 ms of it Slate UI, 0.1 ms animation).
  About a third of the render thread is waiting for visibility work (`EventWait/Visibility`, 2-3 ms).
- **Hitches**: every run has the same two stalls on the walk, 70-300 ms each, at the same places: synchronous loads
  (`FlushAsyncLoading` in the client log) when Volcanic cave 2 comes within 250 m (the cave mesh is loaded by path when
  its actor arrives) and when the Basaltback herd comes within 150 m (a creature's mesh, clips and materials are loaded
  by path when it's first seen). The editor binary also waits for the mesh's render data there, so a packaged build
  stalls less, but it still loads on the spot.
- **Garbage collection**: one pass every 61 s (engine default `gc.TimeBetweenPurgingPendingKillObjects`), the longest
  12-19 ms. On its own that isn't a visible hitch at this frame rate.
- **Streaming**: after the 2.5 km jump the client had the new World Partition cells in within 0.1-0.25 s (landscape
  proxies 8 -> 15 in one quarter-second sample) and no frame over 36 ms on a quiet machine. Not slow in the editor
  binary; see the streaming notes below.
- **Server**: with the usual wildlife the server works 3-4 ms of each 33 ms frame. With every pack out (169 creatures)
  it's 10-14 ms average, p99 15-28 ms, worst 37-135 ms: TickActors (creature bodies and brains) 6-8 ms and character
  movement 2-3 ms are most of it, and 199 idle ride-camera arms sweep for collision every frame. Bringing all 81 sites
  out at once costs one 412 ms frame (169 spawns), which is what logs the server's `Max iterations` warnings.
- **`GetSimulationTimeStep() - Max iterations 8 hit`**: slow frames, not settings. Character movement splits a frame
  into steps of at most 0.05 s, 8 at most, so it warns when one frame is longer than about 0.4 s and drops the rest of
  that time. Every warning in the logs sits on such a frame: the client's own map load / join (creatures on the client,
  `WRCreature_N ... Walking`), the server catching up a player whose client stalled while loading (`WRCharacter_0
  ... Falling`), and the server's 169-spawn burst. Raising the iteration limit would only hide it and do more work in
  an already slow frame, so the fix is fewer slow frames.

## The perf pass (branch polish/perf): what changed and why

Gameplay is the same: no numbers, rules or behaviour near players changed.

1. **Wildlife cost by distance** (`UWRSignificanceSubsystem`, `Source/Wildreach/WRSignificance.*`, numbers in
   `Data/tuning.json` "significance"). On the authority (dedicated server, listen-server host, or a game on its own) a
   creature further than 200 m from every remote player (350 m from a player on the server's own machine, who has the
   whole island loaded and can see further) ticks its body, its movement and its brain every 0.25 s instead of every
   frame, moves in steps of up to 0.1 s (so a quarter-second tick stays inside the engine's 8 steps), and replication
   looks at it 4 times a second. Inside 180 m it's back to every frame. Why those radii: clients are only sent creatures
   within 150 m (the default net cull distance), so a far creature can't be seen or reached by anyone; it keeps doing
   the same things (wandering, hunting other creatures, sleeping, taming progress) in coarser steps. Bosses and ridden
   creatures always run every frame. Creatures are looked at a slice per frame (each once every 8 frames), which also
   spreads their slow ticks over different frames: the first version switched them all in one frame and they then all
   ticked together, average 5.7 ms but p95 16.7 ms (rows "first version"). On a dedicated server their unused skeletal
   meshes tick at the same slow rate. Results, same build and session, all 169 creatures out: off 14.6 ms average /
   p95 23.6 / p99 30.3 / worst 60 / 6 `Max iterations` warnings, on 6.8 / 10.5 / 12.1 / 50 / none (the base measured
   10.4-13.6 average, p99 14.7-28.5). With the usual wildlife (most packs out are near the player; the always-on packs
   and cave colonies elsewhere are what goes far) 3.2 -> 3.0 ms. Players host games themselves (listen server), so on
   the host this work comes out of the frame they're playing in. `-wrtune=significance.enabled:0` turns it off.
2. **Ride cameras only tick while ridden** (same subsystem, every machine): each creature's ride-camera arm swept for
   collision every frame even with nobody on it, 199 sweeps a frame on the all-out server; now only a creature a player
   controls ticks its arm and camera (checked every frame, so mounting gets its camera at once).
3. **Background preload** (`UWRPreloadSubsystem`, `Source/Wildreach/WRPreload.*`, `Data/preload.json`, tuning
   `preload.enabled`). At map start (behind the main menu or the join) every "/Game/..." path in creatures.json,
   player_art.json and items.json, plus the three cave meshes, is requested as one background load and kept for the
   life of the world: 178 assets, loaded in about 6 s. When a creature, a cave, another player's body or a held weapon
   first shows up, its art is already in memory, so the synchronous loads that stalled the walk are gone. Cost: that art stays in memory for the session (about 220 MB of source assets in the editor; less
   cooked, and textures still stream by mip).

**Result on the client** (rows "perf pass", a quiet machine: GPU used by others 0-1%): no frame over 50 ms in any of
the four runs, and no `FlushAsyncLoading` on the walk any more. Epic 7.7 ms (130 fps) both runs, p99 9.3-9.8, worst
frame 23-28 ms; Medium 5.0-5.3 ms (188-201 fps), p99 6.1-7.5, worst 24-37 ms. The base had 2-6 frames over 50 ms on
every run, at the same two spots on the walk (78 ms on its best Epic run, 43 ms on its quietest Medium run, 150-260 ms
when the machine was busy). The steady frame rate is the same as the base on a quiet machine (none of this is
rendering): the base's best Epic run was 8.45 ms with 23% of the GPU used by others; its Medium 5.0 ms quiet.

**Config settings changed: none.** Each candidate below was checked and left at the engine default because the numbers
didn't call for it or it couldn't be verified in this pass.

## Checked, not changed

- **`Max iterations` warnings**: explained above (slow frames). Fewer slow frames is the fix: the preload removes the
  walk's load stalls, and the significance pass took the all-out server from 4-11 warnings per run to none. The one
  412 ms server frame when all 81 sites come out at once only happens in the stress test (in play a player brings a few
  sites out at a time); spreading spawns over frames would mean changing the spawner in WRCreature.cpp, so it's left.
- **Garbage collection**: incremental reachability (`gc.AllowIncrementalReachability`) is off by default and still
  experimental in 5.8 (it needs every object reference to be tracked; a missed one is a crash), and the measured pauses
  (11-20 ms once a minute) don't justify that risk. Not changed. Note for packaged builds: a packaged *Development*
  build (what `Tools\package.ps1` makes) also runs `gc.VerifyAssumptions` on every pass, which the editor binary in this
  benchmark doesn't; a Shipping build or `gc.VerifyAssumptions=0` drops that cost.
- **World Partition streaming**: L_Island uses the 5.8 default runtime hash (one grid, 128 m cells, loading range
  256 m). In every run the new cells were in within 0.1-0.25 s after the 2.5 km jump, with no frame over ~36 ms on a
  quiet machine, so there was nothing to tune here (`wp.Runtime.MaxLoadingStreamingCells`, the level-streaming time
  slices) without a measurement that shows a problem; raising them blindly trades a later pop-in for bigger frame
  spikes. What does look late after a long move: art loaded on the spot (now preloaded), and in packaged builds PSO
  precaching (below), which holds a mesh back until its shaders are compiled the first time. Beyond 256 m clients draw
  no terrain at all until HLODs are built (an open item already).
- **Shader pipeline (PSO) cache for packaged builds**: UE 5.8 already precaches PSOs on D3D12 and holds a mesh back
  until its PSOs are ready (`r.PSOPrecache.ProxyCreationStrategy=1`), so a first-time compile shows as an object
  appearing a moment late rather than a stutter. A bundled cache would cover the rest, but it has to be recorded from
  play and re-recorded whenever materials or effects change, which five agents are doing right now: a cache recorded on
  the base would be stale at merge. Recommended once at release time, after everything is merged:
  1. `Config/DefaultEngine.ini`: `[DevOptions.Shaders]` `NeedsShaderStableKeys=true` (the cook then writes stable
     shader keys, `*.shk`, under `Saved/Cooked/Windows/Wildreach/Metadata/PipelineCaches`), and optionally
     `[ConsoleVariables]` `r.ShaderPipelineCacheTools.IncludeComputePSODuringCook=1` (bundles every compute PSO from
     those keys with no recording; the default 2 bundles only Niagara's).
  2. `Tools\package.ps1`, then play the packaged game with `-logPSO` around the island (camps, the walk, caves, a fight, the menus); it
     writes `Saved/CollectedPSOs/*.rec.upipelinecache`.
  3. `UnrealEditor-Cmd.exe Wildreach.uproject -run=ShaderPipelineCacheTools expand <rec files> <shk files>
     Build/Windows/PipelineCaches/Wildreach_PCD3D_SM6.spc` (and `_SM5`), commit the `.spc` (small), package again: the
     cook builds the bundled cache from it automatically.
  Not set up in this pass because it changes the cook and I couldn't verify a package from this worktree under the
  other agents' load.
- **Cheaper animation for far creatures on clients** (update-rate optimisation): animation is 0.1 ms of the client's
  game thread (clients only have creatures within 150 m), so there's nothing to gain, and it would touch WRCreature,
  which the anim agent is rewriting. Skipped; no WRCreature edit.
- **Client render thread**: the client is render-thread bound and a third of that thread is `EventWait/Visibility`
  (2-3 ms, waiting for the culling / dynamic-mesh tasks). Not changed here (it's the world's content); worth checking
  when the world detail lands: non-Nanite instanced meshes are culled per view on the CPU, Nanite ones on the GPU.
- Seen in the client logs, not mine to fix: `Material /Game/Terrain/M_Rock missing usage flag Nanite! Default Material
  will be used in game` (the cave boulders), and the Slate UI is about 1 ms of the client's game thread.

## Client runs

| Date | Build | Level | Run | avg ms (fps) | p95 | p99 | game | draw | GPU | hitches >50 / >100 | worst ms | GC worst ms (frames) | memory | phases avg/p99 ms | Max-iter | load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-10-26 23:53 | baseline 9c707cb | epic | 1 | 8.45 (118 fps) | 10.3 | 11.7 | 3.3 | 8.4 | 7.5 | 2 / 0 | 78 | 16.6 (9) | 4406 MB | camp 8.0/10.0, walk 8.7/11.9, move 8.0/10.5 (streaming done 0.1 s) | 0 | CPU others 41%, GPU others 23% (Wildreach 13, python 11) (this run 59%); other: 1 UE, 2 game, 0 SCW, 1 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | medium | 1 | 10.94 (91 fps) | 10.3 | 20.2 | 3.1 | 10.9 | 8.2 | 127 / 65 | 1,482 | 17.3 (9) | 4774 MB | camp 955.2/1,057.7, walk 13.8/80.7, move 6.7/11.3 (streaming done 23.2 s) | 0 | CPU others 34%, GPU others 55% (python 26, pid 12332 14, Wildreach 12) (this run 34%); other: 0 UE, 2 game, 0 SCW, 1 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | epic | 2 | 39.35 (25 fps) | 187.1 | 388.2 | 3.2 | 39.3 | 35.7 | 420 / 260 | 4,918 | 14.9 (12) | 5300 MB | camp 28.5/199.7, walk 27.6/356.9, move 207.1/403.5 (streaming done 25.1 s) | 0 | CPU others 31%, GPU others 68% (python 51, Wildreach 8, pid 2492 7) (this run 23%); other: 0 UE, 2 game, 0 SCW, 1 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | medium | 2 | 13.64 (73 fps) | 15.6 | 23.0 | 3.1 | 13.6 | 4.4 | 0 / 0 | 46 | 12.1 (7) | 4560 MB | camp 13.0/23.0, walk 13.8/23.1, move 13.6/17.4 (streaming done 0.2 s) | 0 | CPU others 17%, GPU others 64% (python 58, Wildreach 5) (this run 32%); other: 0 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-27 03:44 | baseline 9c707cb, quiet re-run | epic | 1 | 13.30 (75 fps) | 17.9 | 22.1 | 3.3 | 13.3 | 12.0 | 6 / 1 | 152 | 19.7 (8) | 3703 MB | camp 8.9/19.2, walk 14.9/22.5, move 13.5/20.5 (streaming done 0.2 s) | 0 | CPU others 44%, GPU others 20% (UnrealEditor 19) (this run 43%); other: 2 UE, 1 game, 0 SCW, 0 build, 2 py |
| 2026-10-27 03:44 | baseline 9c707cb, quiet re-run | medium | 1 | 6.16 (162 fps) | 11.6 | 13.5 | 2.9 | 6.2 | 4.7 | 5 / 1 | 258 | 13.8 (7) | 3659 MB | camp 5.0/6.2, walk 6.0/13.6, move 8.5/13.8 (streaming done 0.1 s) | 0 | CPU others 23%, GPU others 15% (UnrealEditor 9, pid 25636 5) (this run 59%); other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 03:44 | baseline 9c707cb, quiet re-run | epic | 2 | 13.85 (72 fps) | 17.1 | 22.2 | 3.1 | 13.8 | 12.5 | 5 / 0 | 91 | 18.1 (7) | 3776 MB | camp 10.8/22.4, walk 14.9/22.4, move 13.5/18.0 (streaming done 0.2 s) | 0 | CPU others 27%, GPU others 24% (UnrealEditor 22) (this run 47%); other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 03:44 | baseline 9c707cb, quiet re-run | medium | 2 | 5.02 (199 fps) | 5.7 | 6.2 | 2.8 | 5.0 | 3.9 | 0 / 0 | 43 | 10.8 (6) | 3591 MB | camp 4.9/6.1, walk 5.1/6.3, move 4.9/6.0 (streaming done 0.1 s) | 0 | CPU others 16%, GPU others 0% (this run 67%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 08:40 | perf pass | epic | 1 | 7.71 (130 fps) | 8.8 | 9.8 | 3.1 | 7.7 | 7.0 | 0 / 0 | 28 | 11.9 (6) | 4135 MB | camp 7.5/9.2, walk 7.9/10.0, move 7.3/9.0 (streaming done 0.1 s) | 0 | CPU others 21%, GPU others 0% (this run 73%); other: 1 UE, 1 game, 0 SCW, 1 build, 0 py |
| 2026-10-27 08:40 | perf pass | medium | 1 | 4.97 (201 fps) | 5.7 | 6.1 | 2.8 | 5.0 | 3.9 | 0 / 0 | 24 | 11.8 (6) | 3923 MB | camp 4.9/6.0, walk 5.0/6.2, move 4.9/6.0 (streaming done 0.1 s) | 0 | CPU others 14%, GPU others 0% (this run 64%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 08:40 | perf pass | epic | 2 | 7.68 (130 fps) | 8.6 | 9.3 | 2.9 | 7.7 | 7.0 | 0 / 0 | 23 | 11.2 (6) | 4085 MB | camp 7.6/9.1, walk 7.8/9.4, move 7.3/8.7 (streaming done 0.1 s) | 0 | CPU others 21%, GPU others 1% (this run 70%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 08:40 | perf pass | medium | 2 | 5.32 (188 fps) | 6.5 | 7.5 | 3.0 | 5.3 | 3.9 | 0 / 0 | 37 | 19.1 (5) | 4043 MB | camp 5.2/7.4, walk 5.4/7.7, move 5.2/6.7 (streaming done 0.1 s) | 0 | CPU others 25%, GPU others 0% (this run 64%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | epic | 1 | 8.26 (121 fps) | 9.3 | 9.9 | 3.4 | 8.2 | 7.5 | 0 / 0 | 24 | 11.1 (5) | 4175 MB | camp 8.2/9.8, walk 8.4/10.0, move 7.8/9.3 (streaming done 0.2 s) | 0 | CPU others 13%, GPU others 0% (this run 68%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | medium | 1 | 5.65 (177 fps) | 6.5 | 7.0 | 3.2 | 5.6 | 4.2 | 0 / 0 | 24 | 12.6 (6) | 4077 MB | camp 5.6/7.0, walk 5.7/7.0, move 5.6/7.0 (streaming done 0.1 s) | 0 | CPU others 14%, GPU others 0% (this run 59%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | epic | 2 | 8.24 (121 fps) | 9.3 | 9.9 | 3.4 | 8.2 | 7.5 | 0 / 0 | 26 | 13.5 (5) | 4159 MB | camp 8.2/9.7, walk 8.4/10.0, move 7.8/9.4 (streaming done 0.2 s) | 0 | CPU others 13%, GPU others 0% (this run 67%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | medium | 2 | 5.67 (176 fps) | 6.5 | 7.0 | 3.2 | 5.7 | 4.2 | 0 / 0 | 25 | 13.8 (5) | 4088 MB | camp 5.6/6.9, walk 5.7/7.1, move 5.6/7.1 (streaming done 0.1 s) | 0 | CPU others 15%, GPU others 0% (this run 59%); other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
<!-- client rows -->

## Server runs

| Date | Build | Scenario | Creatures out | work avg ms | p95 | p99 | worst | frames >50 ms | top categories (ms) | ticks | memory | Max-iter | load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-10-26 22:40 | baseline 9c707cb, first run | usual wildlife (packs within 250 m) | 7 in 3 packs | 2.97 | 3.97 | 5.03 | 41 | 0 | TickActors 0.94, ServerReplicateActors 0.36, CharacterMovement 0.34, HandleRPC 0.28, Physics 0.25, NetworkIncoming 0.11 | ticks/frame 272 (creature 26, movement 33, spring arm 33) | 2036 MB | 0 | CPU others not sampled; other: 0 UE, 2 game, 0 SCW, 0 build, 3 py |
| 2026-10-26 22:40 | baseline 9c707cb, first run | every pack out (169 creatures) | 169 in 81 packs | 13.63 | 20.80 | 28.47 | 135 | 4 | TickActors 7.84, CharacterMovement 2.87, ServerReplicateActors 0.72, Physics 0.44, QueueTicks 0.4, HandleRPC 0.31 | ticks/frame 1,597 (creature 192, movement 199, spring arm 199) | 1999 MB | 11 | CPU others not sampled; other: 2 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | usual wildlife (packs within 250 m) | 7 in 3 packs | 3.82 | 6.26 | 10.45 | 72 | 5 | TickActors 1.33, CharacterMovement 0.52, ServerReplicateActors 0.42, HandleRPC 0.36, Physics 0.24, NetworkIncoming 0.14 | ticks/frame 272 (creature 26, movement 33, spring arm 33) | 1991 MB | 0 | CPU others 26%; other: 0 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | every pack out (169 creatures) | 169 in 81 packs | 10.42 | 13.28 | 14.74 | 37 | 0 | TickActors 5.91, CharacterMovement 2.15, ServerReplicateActors 0.54, Physics 0.37, QueueTicks 0.31, HandleRPC 0.26 | ticks/frame 1,597 (creature 192, movement 199, spring arm 199) | 2010 MB | 4 | CPU others 29%; other: 0 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-27 04:03 | perf pass, first version (slow ticks not spread) | usual wildlife (packs within 250 m) | 7 in 3 packs | 2.62 | 4.05 | 5.90 | 29 | 0 | TickActors 0.66, ServerReplicateActors 0.38, HandleRPC 0.31, Physics 0.23, CharacterMovement 0.23, NetworkIncoming 0.12 | ticks/frame 108 (creature 0, movement 7, spring arm 1) | 0 MB | 0 | CPU others 22%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 04:03 | perf pass, first version (slow ticks not spread) | every pack out (169 creatures) | 169 in 81 packs | 5.65 | 16.68 | 27.92 | 54 | 1 | TickActors 2.62, CharacterMovement 0.77, ServerReplicateActors 0.41, HandleRPC 0.31, Physics 0.29, QueueTicks 0.24 | ticks/frame 439 (creature 0, movement 7, spring arm 1) | 2037 MB | 0 | CPU others 27%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 04:10 | perf pass build, systems off (significance.enabled:0,preload.enabled:0) | usual wildlife (packs within 250 m) | 17 in 7 packs | 3.18 | 6.03 | 8.02 | 47 | 3 | TickActors 0.99, CharacterMovement 0.51, ServerReplicateActors 0.37, HandleRPC 0.26, Physics 0.23, QueueTicks 0.11 | ticks/frame 310 (creature 36, movement 43, spring arm 1) | 2047 MB | 0 | CPU others 24%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 04:10 | perf pass build, systems off (significance.enabled:0,preload.enabled:0) | every pack out (169 creatures) | 169 in 81 packs | 14.56 | 23.56 | 30.28 | 60 | 1 | TickActors 7.61, CharacterMovement 3.75, ServerReplicateActors 0.75, QueueTicks 0.5, Physics 0.47, HandleRPC 0.34 | ticks/frame 1,399 (creature 192, movement 199, spring arm 1) | 2023 MB | 6 | CPU others 21%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 04:17 | perf pass (sliced) | usual wildlife (packs within 250 m) | 17 in 7 packs | 2.97 | 4.53 | 5.05 | 31 | 0 | TickActors 0.82, ServerReplicateActors 0.38, CharacterMovement 0.34, HandleRPC 0.32, Physics 0.23, NetworkIncoming 0.13 | ticks/frame 128 (creature 0, movement 7, spring arm 1) | 0 MB | 0 | CPU others 21%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 04:17 | perf pass (sliced) | every pack out (169 creatures) | 169 in 81 packs | 6.82 | 10.47 | 12.12 | 50 | 1 | TickActors 3.15, CharacterMovement 1.11, ServerReplicateActors 0.47, HandleRPC 0.35, QueueTicks 0.28, Physics 0.28 | ticks/frame 439 (creature 0, movement 7, spring arm 1) | 0 MB | 0 | CPU others 28%; other: 2 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | usual wildlife (packs within 250 m) | 7 in 3 packs | 2.03 | 2.43 | 3.28 | 24 | 0 | TickActors 0.44, ServerReplicateActors 0.29, HandleRPC 0.23, Physics 0.22, CharacterMovement 0.17, WorldTickMisc 0.09 | ticks/frame 76 (creature 0, movement 7, spring arm 0) | 0 MB | 0 | CPU others 12%; other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
| 2026-10-27 10:03 | 0.4.0 merged polish | every pack out (169 creatures) | 169 in 81 packs | 4.23 | 5.26 | 7.33 | 27 | 0 | TickActors 1.77, CharacterMovement 0.6, ServerReplicateActors 0.32, Physics 0.26, HandleRPC 0.24, WorldTickMisc 0.21 | ticks/frame 241 (creature 0, movement 7, spring arm -) | 0 MB | 0 | CPU others 12%; other: 0 UE, 1 game, 0 SCW, 0 build, 0 py |
<!-- server rows -->
