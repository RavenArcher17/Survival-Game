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
<!-- client rows -->

## Server runs

| Date | Build | Scenario | Creatures out | work avg ms | p95 | p99 | worst | frames >50 ms | top categories (ms) | ticks | memory | Max-iter | load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-10-26 22:40 | baseline 9c707cb, first run | usual wildlife (packs within 250 m) | 7 in 3 packs | 2.97 | 3.97 | 5.03 | 41 | 0 | TickActors 0.94, ServerReplicateActors 0.36, CharacterMovement 0.34, HandleRPC 0.28, Physics 0.25, NetworkIncoming 0.11 | ticks/frame 272 (creature 26, movement 33, spring arm 33) | 2036 MB | 0 | CPU others not sampled; other: 0 UE, 2 game, 0 SCW, 0 build, 3 py |
| 2026-10-26 22:40 | baseline 9c707cb, first run | every pack out (169 creatures) | 169 in 81 packs | 13.63 | 20.80 | 28.47 | 135 | 4 | TickActors 7.84, CharacterMovement 2.87, ServerReplicateActors 0.72, Physics 0.44, QueueTicks 0.4, HandleRPC 0.31 | ticks/frame 1,597 (creature 192, movement 199, spring arm 199) | 1999 MB | 11 | CPU others not sampled; other: 2 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | usual wildlife (packs within 250 m) | 7 in 3 packs | 3.82 | 6.26 | 10.45 | 72 | 5 | TickActors 1.33, CharacterMovement 0.52, ServerReplicateActors 0.42, HandleRPC 0.36, Physics 0.24, NetworkIncoming 0.14 | ticks/frame 272 (creature 26, movement 33, spring arm 33) | 1991 MB | 0 | CPU others 26%; other: 0 UE, 2 game, 0 SCW, 0 build, 2 py |
| 2026-10-26 23:53 | baseline 9c707cb | every pack out (169 creatures) | 169 in 81 packs | 10.42 | 13.28 | 14.74 | 37 | 0 | TickActors 5.91, CharacterMovement 2.15, ServerReplicateActors 0.54, Physics 0.37, QueueTicks 0.31, HandleRPC 0.26 | ticks/frame 1,597 (creature 192, movement 199, spring arm 199) | 2010 MB | 4 | CPU others 29%; other: 0 UE, 2 game, 0 SCW, 0 build, 2 py |
<!-- server rows -->
