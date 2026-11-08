# Changelog

## Not released yet: door keypads

From checking the build plan's first playable against the game (Raven: "is the first playable done ... with building?"):
everything in its Building row was in but "doors and code locks", whose codes only the console set or entered
(`WRSetCode` / `WRCode`), so a player couldn't lock a door at all. The dedicated server, the other gap, is on hold
(Raven).

- **A keypad on code-lock doors** (the reinforced door; new screen `WRUIDoor.cpp`, `EWRMenu::Door`). E on a locked door
  that isn't yours brings it up: the prompt says "Enter code". Four digits by click or by the number keys (number row or
  pad), Back / Backspace, Clear / Delete, **Unlock** or Enter. A wrong code shakes the digits ("Wrong code"); the right one
  opens the door and closes the keypad, and the door is yours to open (and shut) from then on, until its code changes.
- **Your own door: hold E** (0.35 s, `ui.doorHoldSeconds`) for its keypad; a tap still opens or shuts it. The prompt
  says "Open · Hold: code lock". Type four digits and **Set code and lock**; **Remove lock** takes it off ("anyone can
  open it"). The same for your clan's doors. Owner and clan are known from the replicated player states, so the hold
  and the prompt work on a friend's machine too; the server decides as before.
- Server: `setcode` / `code` / new `clearcode` take the door's spot (`code 4821 X Y Z`: the keypad's own door, not just
  the nearest piece), the right code opens the door, and `AWRBuildPiece::UnlockedFor` (who has entered it) replicates
  so the prompt and E know who may open it. Every attempt is logged (`DOOR <who> <cmd> at <spot>: <result>`).
- Testing: `WRKey <key> hold <seconds>` holds a key down. New `Tools/doortest.ps1` (alltests `door`, 14 checks, all
  pass): a host with no window and a friend at 1440x900 by real keys and clicks: the host's door, a wrong code by keys
  and the right one by clicks; the friend's own door locked by a held E and keys, the host kept out (his E brings up its
  keypad), a tap still opening it for its owner, the lock removed. buildtest's raider now sends `WRCmd interact` for
  the server's door rules (its E brings up the keypad now). unit, build, survival, house, menus, base and flicker pass.

## 0.7.0: creature raids, crafting time, far views

**Released 2026-11-08** as [v0.7.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.7.0) (Latest; commit
6ac25f3, `C:\Dev\WildreachBuild7`, the Setup's SHA-256 on GitHub the same as the local file's; notes
`Docs/release-notes/v0.7.0.md`). It carries 0.6.3's base fixes too, which never went out on their own. The plan Raven approved after 0.6.3: creature raids, crafting time with station queues, far views, ash-grey
volcanic shrubs, ice rebuilt like the lava. All done but the shrubs, which still hang the GPU and stay green (Raven:
"leave the shrubs green if it's a problem"). Then a polish pass, and boss fights from Raven's play.

- **Bosses chase you, and walk home instead of jumping there** (`AWRBoss::ChaseRadius`, tuning.json `bosses`). Raven:
  "the radius is too small and it's very hard to fight a boss because it can't walk far and it glitches back or doesn't
  pursue". A boss only fought within its arena's floor (`arenaRadius`, 30 m for a major boss, 22 m for a mini one):
  past it, it stopped coming after you, and 8 s with nobody inside it teleported home at full health. Now it chases and
  counts fighters within `chaseRadiusMajor` 90 m / `chaseRadiusMini` 65 m of home (flat distance; the floor stays for
  floods and props). With nobody there for `resetSeconds` it still resets, but walks (or flies) home; only one that
  isn't home after `returnStuckSeconds` (25) is put back. On its way home it only wakes for someone it may fight.
  New `Tools/bosschasetest.ps1` (alltests `bosschase`, 5 checks): Grimjaw follows the player 35 m from home and keeps
  fighting, then resets with them 200 m off and walks home, full health, no teleport. Dev command `bossinfo`.
- **The raid tracker** (`AWRPlayerController::ClientRaidTrack`, `WRUIHud` under the level plate): while a raid is on
  your claim, "Raid on your base" with a clock: "5 coming from the east" until they come, then "4 of 5 raiders left"
  with a bar, until it's over. Sent each raid step to everyone of the claim; it goes when the updates stop.
- **The raid card no longer has a notice showing through it.** The warning came as a notice and a card in the same
  place; the card carries it alone now. And any notice that comes while a card or banner is up (a boss's, a level-up)
  steps down under it instead of sitting behind it ("Your 8 Copper ingot are done" did, in the friend's test).
  The flow screen's toasts move down under the raid tracker while it's up.
- **Shared stations: whose order is whose.** In the forge's strip someone else's order says "Mason's order", and its
  **Cancel** shows only when you may cancel it (yours, or any at your own or your clan's station: the server's rule).
- **The Crafting tab at 4:3 and 5:4:** cards go two a row when three would be too narrow for their names ("Bronze ingot"
  ran under its lock).
- **Two-player testing**: bots keep in step by chat (`waitchat <text>` waits for a chat line). New
  `Tools/cooptest.ps1` (alltests `coop`, 12 checks, all pass): Mason hosts (no window) and builds a base, Bravo joins in
  a 1024x768 window and plays by clicks: Bravo's 8 ingots and Mason's 2 swords on one forge (Bravo sees Mason's order
  without Cancel, then with it once in Mason's clan; each one's things to their own bag or the tray), then a raid with
  Bravo getting the card, the tracker coming and counting, and "Raid over" when Mason kills them.

Checked before release, on commit e6c4fe2:
- the full suite: 36 of 37 passed; `menus`' host hit the known GPU hang (`DXGI_ERROR_DEVICE_HUNG`, VSM Nanite
  `NodeAndClusterCull`, two game windows on the GPU) and passed run alone, so all 37 pass;
- the packaged build (`C:\Dev\WildreachBuild7`, Setup 1,682 MB, 667 files, 16 changed from 0.6.2): a clean install
  from it matches the build in every file; **0.6.2 with its updater** (four connections) and **0.6.1 with its updater**
  each fetched only what changed, 236 MB, all 667 files right, the save kept, nothing left over, the new updater in
  place (a local stand-in for GitHub serving the Setup);
- the packaged game, updated from 0.6.2: crafttimetest's steps by clicks (10 of 10 on its own log) and raidtest's (14
  of 14); a copy of Raven's "playtest 0.6.0" world loads from the main menu (14 pieces, 368 animals, raids "rare", all
  257 landscape pieces, the Claim tab showing the totem and the Hearth), no errors. Raven's real worlds untouched.

- **Ash-grey volcanic shrubs: retried, still off.** The plan was to try `world.ashCoatMin` 0.85 again, since the hang
  it gave in 0.4 (4 of 4 benchmark runs) looked like the Nanite tessellation one fixed in 0.5.0. It isn't: with it, 2 of
  3 Epic runs hung (`DXGI_ERROR_DEVICE_HUNG` in the virtual shadow maps' Nanite `NodeAndClusterCull`), and the 5 runs
  without it the same day were clean (Docs/PERFORMANCE.md). It stays at 0; greying them needs another way than the
  pack's snow coat (the note in `WRWorldDressing.cpp`).

- **The ice, rebuilt like the lava.** It was flat 6 m squares (the hazard cells, 13,576 of them) floating 20 cm over the
  ground. Now it's a sheet on its own 2 m grid (`hazards.iceGrid` in `Data/island_dressing.json`, made by
  `Tools/terrain/dressing.mjs` from the same cells: a distance pass gives how far inside the edge each point is, with a
  ragged, noisy rim; 134,424 points), a thin skin over the ground that sinks under it at the rim, drawn in 454 chunks
  that each cull alone (`AWRDressing::BuildIceSheet`, sharing the lava's `BuildGridSheet`). `M_Ice` (rebuilt by
  `build_terrain_materials.py`, `WR_MATERIALS=ice`) is glassier: deep teal-blue and clear where it's thick, frosted at the
  rim, a mirror-smooth top (roughness 0.03, specular 0.8) with faint frozen ripples, a cool sheen at grazing angles, and
  the lava's far lift. What's slippery is still the 6 m cells, so play is as before. Checked in pictures from above, the
  side and low, by day, dusk and night, on a river and the glacier ring. Everything else in the dressing file is
  byte-for-byte what it was.

- **Far views: a friend who joins sees the whole island, as the host does.** A server loads every cell of the map
  (server streaming is off), so the host always had the whole island; a player who joined loaded the ground within
  `world.streamingRangeScale` x 256 m, 1 km at x4. Standing 1.3 km west of the middle, a friend had 59 of the 257
  landscape pieces and the far peaks were missing from their view (the host, at the same spot, had them all). x24 (6 km,
  the whole island from anywhere: 5.7 km corner to corner) is the default now. Benchmarked on Epic, a joined client:
  8.33 ms / p99 10.7 / 4,361 MB at x4, 8.38 ms / p99 10.7 / 4,654 MB at x24, and nothing left to stream in after the
  2.5 km move (Docs/PERFORMANCE.md). HLODs aren't needed for it. New `WRStreaming` logs what this machine has loaded;
  worldstest checks the friend has all 257 pieces.
- **A crash when the host changes the world's settings, fixed.** The animation workers read two tuning numbers
  (`anim.footMaxDrop` / `footMaxRaise`, not in tuning.json, so the fallbacks) through `FWRTuning::Num`, which adds a
  missing key to its warn-once set: two threads adding at once, or the host's settings change reloading the tuning at
  that moment, corrupted it (`Assertion failed: Index>=0 && Index<NumBits` in a TSet, the worlds test's host, a friend
  in the game). They're read on the game thread now (`FWRAnimFrame::FootMaxDrop / FootMaxRaise`), and `Num` only keeps
  that set on the game thread. An old race, not new in 0.7.

- **Crafting time with a queue at each station** (`AWRStation::Queue` / `Output`, tuning.json `craft`). A station's
  recipe now takes its time (recipes.json `seconds`, 2-7 s, there since the first drafts but read by nothing) over the
  world's crafting speed (`server.craftMultiplier`, also read by nothing until now: 2x in Casual co-op and Playtest, so
  1-3.5 s). Craft with a quantity is one order: its inputs are taken when it's ordered, and the station makes them one
  after another. It keeps going with nobody at it; each one goes into its maker's bag while they're within 20 m, else
  into the station's tray (8 slots). With no room for it, it waits. Up to 6 orders a station, 50 of one recipe an
  order; a broken Hearth stops them where they are. Hand recipes stay instant.
  - The Crafting tab: the details say how long ("Takes 1 s each: 5 s for 5. It keeps going if you walk off."); under the
    cards, a strip with the station's orders (the first with its bar, "4 of 5"), each with **Cancel**, and what's
    **Ready** in the tray with **Take all**. A notice when an order you walked off from is done.
  - Cancel gives back what wasn't made (the one under way too), to its maker or the base's owner and clan; what doesn't
    fit lands in a bag at the station. A station smashed or demolished leaves its tray and its orders' materials in a bag.
  - The quality roll, perks and double brews use the maker's skill when they ordered it, so a queue finishes the same
    after they've walked off or logged out (skill XP comes as each one is made, while they're on). Saved with the
    station (`queue`, `tray` in its piece in world.json); the one under way keeps the time it had left.
  - `FWRCrafting::Make` is split into `Shortfall` / `TakeInputs` / `HandOf` / `MakeOne`, shared by the queue and the
    instant path (`craft.timeScale` 0: crafttest and survivaltest run that way, as they test what's made). New
    commands `craft <id> <element> <times>`, `craftcancel <n>`, `crafttake`, and `saveworld` (tests).
  - `Tools/crafttimetest.ps1` (alltests `crafttime`, 10 checks, all pass): by clicks at a forge, five ingots one at a
    time about a second apart into the bag, ten more walked off from going into the tray with the notice, Take all,
    two swords cancelled with their materials back, and an order under way saved with its forge.

- **Creature raids** (new `WRRaids.cpp/.h`, numbers in tuning.json `raid`). The world setting "Creature raids" (Off /
  Rare / Normal / Frequent; `server.raids`, already in every preset but read by nothing until now) is on the world
  settings screens (new world and Esc > World): a claim's raid comes every 72 / 24 / 12 game hours or so (give or take
  a quarter), counted per claim from when it's first seen (`AWRClaimTotem::NextRaidHours`, saved with the totem as
  `nextRaid`; claims from older saves get their first one interval after this update loads them).
  - Only while someone of the claim (its owner or their clan) is home: a due raid waits until one of them is within
    250 m of the totem (`raid.homeRange`), since creatures only exist near players. Nobody's base is raided while
    they're away, and a raid ends (the rest leave) if everyone of the claim goes offline.
  - A minute's warning (`raid.warnSeconds`) to everyone of the claim who's on, wherever they are: a card like a boss's
    with the raiders' portrait ("RAID / CINDERFANGS / 5 Cinderfangs are coming for your base from the east, in 60 s")
    and a notice. Then they come from 30 m outside the claim's edge, on dry ground.
  - Who: the claim's region's hunters (`Data/spawn_regions.json` new `raiders`: Cinderfangs in the volcanic lowland,
    Cinderfangs and Scorchwings in the midland, Magmaws and Cinderfangs in the highland; Gloomhowls, Glacimaws, Rimeclaws
    on the tundra side), rolled by weight. How many: 3 points plus 2 per Hearth level (no Hearth: 3; a new Hearth at
    level 1: 5; level 3: 9), a lowland hunter costing 1, midland 2, highland 3 (`WRRaids::Plan`).
  - What they do (`AWRCreatureAI::TickRaid`): each goes for the nearest piece of the claim (foundations and the totem
    count as further off, so walls, doors, chests and stations go first; a broken Hearth is left alone) and bites it:
    its bite x 1.5 every 1.6 s (a Cinderfang: 18), with the bite's wind-up and its dust or splinters. Anyone who comes
    near is attacked as any hunter would. They never sleep or go after prey, and their leash runs from the base.
  - The end: all killed ("Raid over: you held them off, 2 pieces lost"), or after 6 minutes the rest walk off and are
    taken away out of sight. The claim's next raid is then one interval away. A raid in progress isn't saved.
  - Testing: `WRCmd raid now [warn s] [from bearing] | due | status | plan | end`.
- **A broken chest leaves what it held in a bag** (`AWRLootBag::Spill`, `building.spillMinutes` 30, anyone's to take).
  A chest smashed, demolished or decayed used to take everything in it with it.
- Tests: `Tools/raidtest.ps1` (alltests `raid`, 14 checks, all pass): the setting read, the first raid an interval
  away, the plan by Hearth level, a raid from the east (the card and notice, 5 Cinderfangs for the level 1 Hearth, 24
  bites, the chest broken first and its things in a bag, the player bitten on walking in, killed: held, next scheduled),
  a due raid waiting while the player is 400 m off and coming when they're back, timing out and the rest taken away, and
  the next raid saved with the claim. Automation `Wildreach.Raids.Plan` / `.Regions`. worldstest clicks "Frequent" on
  a new world's form and finds `"raids": "frequent"` in its settings.json.

## 0.6.3: your base (the Hearth found, repaired; chest names, waypoints)

**Not released on its own: it goes out in 0.7.0** (its notes, `Docs/release-notes/v0.6.3.md`, are folded into 0.7.0's).
From Raven's play of 0.6.2: "there was nowhere I saw to
build the Hearth or upgrade it". The build bar's card row was a horizontal scroll box exactly three cards wide with a
4 px scroll bar; the Stations tab had eight cards and the Hearth was the eighth (the claim totem the seventh), so neither
was ever on screen. Most tabs overflowed the same way (stone walls, floors, roofs). No test caught it: every test placed
the Hearth by command (`WRGhost hearth`), never through the bar.

- **Build bar** (`WRUIBuild.cpp`): a **Claim** tab holds the claim totem and the Hearth. The cards go a page of three at
  a time (`PerPage`): round ‹ › arrows on the row's ends, level with the pictures (shown only when there's a page that
  way), the mouse wheel over the cards (`SWheelArea`), and a "4–6 of 6" tag on the row's top edge. Placing a piece turns
  the row to its page. The tab strip shrinks to fit on narrow screens (`SFitWidth`; at 16:10 the eleven tabs ran into the
  Demolish button, and SScaleBox's ScaleToFitX only clipped them: it sizes from the last frame's paint geometry and
  never scaled down here). The Hearth's card says "E on it: feed it major bosses' cores".
- **First-claim hint** (`FWRFlow::CheckHints`, id `hearth`): standing in your own claim with no Hearth covering it, "Your
  claim is up. Build a Hearth in it (build menu, Claim tab), then E on it: boss cores raise every station's tier."
- **A broken Hearth stays and can be repaired.** The Hearth's build piece took the damage and was destroyed the moment
  it reached 0, cores and all; the Hearth actor's own "broken" state was never seen, and `AWRHearth::Repair` was never
  called (the map even said "repair it to craft"). Now the piece stops at 0 and the Hearth's health follows the piece's
  (`SetHealthFraction`; loaded from the saved `hp` too), so a broken Hearth stands, stops its stations and keeps its
  cores. The Hearth screen has **Repair** under its health while it's damaged: `hearthrepair` → `AWRHearth::RepairFor`,
  half the piece's build cost scaled by the damage (`hearth.repairShare` 0.5; 15 stone + 3 copper ingots when broken),
  owner and clan only.
- **Chest names**: `AWRBuildPiece::CustomName` (replicated, saved as `"name"`), `chestname <words>` (24 characters,
  owner and clan), the chest screen's **Rename** opens a name box over the screen that takes the keyboard (UI only, as
  chat does; focus a frame after it opens, or the Rename button takes it back), Enter / Save / Esc / Cancel.
- **Map waypoints**: right-click the journal map (`WRUIMap::SetWaypoint`; right-click it again clears it); a brass pin
  with its distance on the map and on the HUD compass, waiting at the compass's end with an arrow when it's behind you.
  This client's own, for the world it was set in (not saved). The map's controls row lists it.
- **Your base on the map from anywhere**: claim totems and Hearths (and their build pieces) are `bAlwaysRelevant`, so a
  friend's own Hearth and claim reach them across the island (they replicated only within 150 m and dropped off the
  map). The map shows someone else's claim only within 150 m, as before.
- Testing: `WRMouseClick ... once` (no take-back click first: a second click would undo a toggle like the waypoint).
  New `Tools/basetest.ps1` (alltests `base`, 12 checks) does all of the above by real keys and clicks as the host at
  1440x900, 16:10: 12 of 12 pass.

Checked on the packaged build (`C:\Dev\WildreachBuild6`, Setup 1,681 MB, 667 files, 9 changed from 0.6.2) against a local
stand-in for GitHub serving its Setup:
- a clean install from it matches the build in all 667 files;
- **0.6.2 with its updater** (four connections): 9 of 667 files changed, **231 MB**, all 667 files right, the save kept,
  no working files left, the new updater in place; **0.6.1 with its updater**: 12 files changed, the same 231 MB, all right;
- the packaged game, updated from 0.6.1, passes basetest's 12 checks on its own client log (the same steps by keys and
  clicks), and loads a copy of Raven's "playtest 0.6.0" world from the main menu (14 pieces, 154 packs, the Claim tab
  showing the totem and the Hearth). The only error is the engine's known PSO-precache ensure on the frame Play is
  clicked. Raven's real worlds untouched;
- the full suite on this code: 30 of 33 passed; the three that failed in the loaded run all passed run alone (`menus` 12
  checks, `flow` 22, `roam` 25), so every test passes. The `menus` host's GPU hung (`DXGI_ERROR_DEVICE_HUNG`) 5 s after
  the friend joined, with two game windows on the GPU: not the 0.5.0 hang (Nanite tessellation's `PatchSplit`) but the
  virtual shadow maps' Nanite `NodeAndClusterCull`, the same place as all 7 earlier hangs since 0.5.0 (`menus` on 11-04,
  the creature-video, size-check and portrait tools), every time with the GPU heavily loaded. Not new in 0.6.3; not
  looked into yet.

## 0.6.2: updates download over four connections

**Released 2026-11-07** as [v0.6.2](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.2) (Latest; commit
16984a9, `C:\Dev\WildreachBuild5`). A copy of 0.6.1 updated itself from the real release in 32 s (235 MB, 12 of 667
files, all 667 right, the save kept, nothing left over). The first release players on 0.6.1 get as a small update.
Before release, checked on the real build against a local stand-in for GitHub serving its Setup (1,681 MB, 667 files;
the new one is `Data/spawn_regions.json`):
- a clean install from it matches the build in all 667 files;
- **0.6.1 with its own updater** (what players have): **230 MB** (12 of 667 files changed), 13 s here, all 667 files right,
  the save kept, no working files left, and the new updater in place;
- **0.6.0 with its updater** (Raven's install): the whole Setup, as that updater always does, all 667 files right;
- the packaged game on a copy of Raven's "playtest 0.6.0" world (saved by 0.6.0: 154 packs, 467 animals, packs up to 11)
  loaded as 368 animals, every pack in a region; over a couple of minutes a periodic check added a Magmaw to the volcanic
  highland (6 of 7) and the regions over target thinned; saved as 149 packs, 363 animals, none over 4. Island (never
  played with wildlife) and a new world each filled every region (348 and 351 animals). The only error in the logs is the
  engine's known PSO-precache ensure on the frame Play is clicked. Raven's real worlds untouched.
- the full suite (`Tools/alltests.ps1`) on this build: 30 of 32 passed; the two that failed in the loaded run both passed
  run alone (`worlds` 31 checks, `updater` 44 checks), so every test passes.

GitHub gave this machine about 140 KB/s a connection when 0.6.1 came down this morning (22 minutes for 234 MB), and four
connections at once each got the same.

- **The updater now downloads over 4 connections at once** (`Tools/installer/WildreachUpdater.cs`, `Connections`). The
  changed blocks go in runs of no more than 8 MB, so one big changed file (the game's exe) is shared out too, and four
  threads take the next run as they finish. A dropped connection still picks up at the block it was on.
- **The whole Setup comes down in parts as well** (8 MB each, written into place) whenever the small way only wasn't worth
  it (no index, or over 80% changed). If parts of the Setup came down wrong or the server won't do partial downloads, it
  still asks for the whole file in one go. The Setup checks itself when it runs, as before.
- .NET's default of two connections to a server is raised to eight.
- **Measured:** GitHub at 1, 4 and 8 connections this afternoon: 5.5, 16.9 and 11.6 MB/s in all, so four, not eight. The
  real update off GitHub (a copy of 0.6.0 to v0.6.1, 35 parts) came out matching the 0.6.1 build in all 666 files. This
  afternoon GitHub was quick enough that the line was the limit: the old updater and the new one both took 27 s. With
  every connection held back, as GitHub does at a busy time (the offline test, each about 1.4 MB/s), the four together
  went 3.3 times as fast as one: 4,789 KB/s.
- `Tools/updatertest.ps1` (44 checks, all pass): the small update stays at 4 connections or fewer, the whole Setup in parts
  (with a dropped connection in the middle), and every connection held back.

## 0.6.2: wildlife spawns by region

Raven asked for spawning that works like ARK's ("define regions, give each a weighted creature list plus a population
target, and run a periodic check that refills missing slots away from players and structures"), and small packs: "small
packs like 3-4 feel normal for pack animals".

- **Regions** (`Data/spawn_regions.json`, new): the island's two sides, each in its three bands (lowland, midland, highland),
  so six. Each has a population target and its creatures, each with a weight (the chance a new pack is of that kind). The
  weights are the old site counts, so the mix is as before; the targets are those sites times the new pack sizes: volcanic
  133 / 57 / 7 and tundra 74 / 66 / 7 (low / mid / high), 344 animals in about the same 156 packs. A region can also be cut
  down to a box (`area`), for a valley of its own later. `server.creatureNumbers` still scales every target.
- **A new world fills every region** to its target, each pack's kind rolled by weight, somewhere that kind can live (its
  ground on the walk map, clear of the camps).
- **The periodic check** replaces breeding and wandering in: every `wildlife.spawnCheckSeconds` (60) a region short of its
  target gets up to `wildlife.spawnPacksPerCheck` (2) new packs, never within `wildlife.spawnAwayFromPlayers` (300 m) of a
  player, `wildlife.spawnAwayFromBuildings` (100 m) of anything built, or in a land claim. Each one is logged
  (`WILDLIFE spawned ashstrider x3 in volcanic_lowland at ...; 412 m from a player, nothing built`), and the step log has
  each region's count against its target. A region over target (creature numbers turned down) still thins out of sight.
- **A pack belongs to its region** wherever it roams (saved as `region` in `wildlife.json`), so a herd that's wiped out is
  made up for by a new pack somewhere else in that region, not by the survivors breeding back. A pack that's down to none
  is gone, burrowers too.
- **Small packs** (`creatures.json` `pack`): Cinderfang, Gloomhowl, Ashstrider, Emberhorn and Frostantler 3-4 (they were up
  to 6, 7, 12, 8 and 8); Basaltback, Woolly Drudge, Glowmoth, Pumicehog and Snowpuff 1-3; Kilnmite and Rimeshell 2-3;
  Magmaw, Rimeclaw, Scorchwing and Aurora Fox 1-2; Glacimaw alone. Nothing wild comes in more than 4.
- **Old worlds:** a pack saved bigger than its kind allows now is cut down to the most it can be, and a pack with no region
  joins the one for its kind where it stands. The regions then make up any shortfall with new small packs.
- `wildlife.refillPerHour` and `wildlife.arriveDistance` are gone. `Data/island_spawns.json` stays as reference ground (the
  walk map and the tests stand on its sites).
- **Tests:** `Wildreach.World.SpawnRegions` (every region on its side and band, every wild kind in one, targets);
  `Wildreach.World.WalkMap` now wants packs of 4 or fewer. `Tools/roamtest.ps1` checks a new world against each region's
  target, pack sizes, kinds by region, an emptied region refilling (a player and a building in it), at most 2 packs a
  check, and the distances. `Tools/wildlifetest.ps1` seeds its two herds in a saved world; `Tools/rulestest.ps1` compares the
  share and the animals (the pack count is now only about in proportion).
- **Results:** unit 28/28, roam 25/25, wildlife 11/11, rules 8/8, water 13/13, gather 19/19, survival 25/25, ai 11/11,
  tame 10/10. A new world: 148 to 157 packs, 347 to 353 animals for a target of 344; packs x1 42-52, x2 35-41, x3 49-51,
  x4 16-21. Volcanic lowland emptied with a player in it refilled 0 to 133 in 33 checks, at most 2 packs a check, the
  nearest new pack 309 m from the player and 605 m from a building (600 m asked for the test). A copy of Raven's 0.6.0
  world (154 packs, 467 animals, 31 packs over 4) loaded as 368 animals, every pack in a region, no errors.

## 0.6.1: much smaller updates

Raven asked why an update is another 1.7 GB when the game updates itself. It was downloading the whole Setup every time.
**Released 2026-11-07 as [v0.6.1](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.1)** (Latest, the
same 1.7 GB `Wildreach-Setup.exe` for everyone this once, notes `Docs/release-notes/v0.6.1.md`; GitHub's digest matches the
build). Then the real thing, off GitHub: a copy of 0.6.0 with the new updater fetched **234 MB** (11 of 666 files changed)
and came out matching a clean 0.6.1 install file for file. It took 22 minutes: GitHub served this machine about 140 KB/s a
connection (another site, 2.2 MB/s), and four connections at once each got the same, so fetching in parallel would be about
four times quicker.

- **What actually changes between builds** (0.6.0 against 0.6.1, compared block by block): the content file, 1,383 MB, came
  out byte for byte the same. Nearly all of a code update is `Wildreach.exe` and `Wildreach.pdb`: one change shifts addresses
  all through them, so they're effectively new each time (120 MB and 112 MB compressed). Everything else is small.
- **The Setup is now its own patch source** (`Tools/installer/WildreachPack.cs`). Its game is no longer a zip: every file is
  cut into blocks of about 256 KB at places chosen by the bytes (content-defined chunking, so a change only disturbs the
  blocks around it, even when everything after it moves), each compressed and stored once, with an index and a footer on
  the end of the program. A release is still one file, with nothing else to upload, and a new player's install is the same
  as before. `Tools/installer/WildreachPackTool.cs` makes it, in under a minute.
- **The updater fetches only what's missing**, with exact-range requests to that same file. (GitHub's store serves those,
  through the redirect, but not "the last N bytes", so it asks the length first.) It cuts its own files the same way,
  downloads the blocks it hasn't got, builds each new file beside the old one and checks its hash, then swaps them in with
  `version.txt` last, so a copy that stops part way still says it's the old version and the next start finishes the job.
  Fetched blocks survive an interruption. Files a player made are never touched: only what's on the last version's
  `installed-files.txt` can be removed, and an index that names anything under a `Saved` folder is refused outright.
- **If the small way can't be done** (no partial downloads, a damaged block, an old-style Setup, or over 80% changed) it logs
  why and downloads the whole Setup as before. A file in use puts everything back and says so, without that download.
- **Measured on the real builds, each ending with all 666 files matching a clean install:** a code update (0.6.0 to 0.6.1)
  **235 MB** of 1,681 MB, 13 s here; a settings-only patch (one number in `tuning.json`) **295 KB**, of which 28 KB is the
  change and the rest the index. The real jump from 0.5.0 to 0.6.0, every creature rebuilt, would have been about 316 MB.
- **0.6.1 itself is still the full download,** because the updater every player has is the old one. That old updater was
  tested taking the new Setup, on the real build. Small updates start with the next release.
- **Old Setups no longer pile up.** Every full update left its 1.7 GB Setup in `%TEMP%\Wildreach-Update`: deleting it straight
  after it ran failed silently, most likely because a virus scanner was still reading it. The updater now clears that folder
  when it starts, so leftovers from 0.6.0 and earlier go too.
- `Tools/updatertest.ps1` now covers all of it: the small update, each fallback, a dropped connection, the updater killed part
  way, half swapped, a file in use, the updater and Setup from 0.6.0.

## 0.6.1: aiming and building feel

From Raven's first feedback after 0.6.0.

- **A crosshair**, always up: a dot and four short ticks at the middle of the screen, red over something hostile. Settings >
  Controls can turn it off (it still shows while aiming, placing or in first person). `ui.crosshairFoeRange`.
- **Spells go where it points.** Bolts flew parallel to the camera from the chest, so with the camera behind the shoulder
  they passed beside what you aimed at. They now leave the chest through the point under the crosshair
  (`AWRCharacter::CrosshairPoint`, which works on the server); Lava pool and Meteor land on it. 6 hits from 6 at 15 m.
- **Locked on, spells hit what's moving.** The lock lived only on the client, so the server just fired along a lagging
  camera. Now it knows (`ServerSetLock`): bolts aim ahead of where the target's going (`UWRCombatLibrary::LeadPoint`) and
  curve toward it (`AWRProjectile::HomeOn`; `spells.lockHoming`, less for other players so they can dodge). Lava pool lands
  under the target, Meteor where it'll be when it lands. At a player always running across at 15 m, turning back at each end
  of a 24 m lane: 7 to 10 bolts from 10 and 3 to 4 Meteors from 4; with the lead and the curve turned off
  (`aimtest.ps1 -NoAssist`) 1 bolt from 10 and 1 Meteor from 4.
- **Lock-on took things you couldn't see.** It never checked for buried burrowers, nor for anything in the way. Every
  volcanic cave has a buried Kilnmite colony (2 right in front of the camera in the cave tested), so it could turn the view
  to a bare patch of cave or the lava beside it. Only a host had this: a buried creature is hidden with no collision, so it's
  never sent to a client who joins. It now
  takes what's visible with a clear line (`CanSeeTarget`), nearest the crosshair rather than nearest you, and lets go if the
  target goes underground or stays out of sight (`camera.lockOn*`). What's locked gets four gold corners on screen: the new
  UI had no marker at all.
- **Pieces follow you.** The angle was set once, when you picked the piece. It now faces the way you look, plus however
  far you've turned it. Free (no grid under it), any angle: the rotate key turns it 15 degrees, held it spins, the wheel
  turns it 5 (`building.turnStep`, `spinSpeed`, `wheelStep`).
- **Snap has three settings.** F goes Grid, Fine, Off. Furniture on a foundation or floor now snaps to that piece's marks
  and sits square to it: 100 cm and quarter turns on the grid, 50 cm and eighths on the fine grid. The fine grid also takes
  walls on inner lines 100 cm apart and pillars on their crossings. Marks show on the piece you're aiming at.
- **A foundation placed at an angle has its own grid** (`FWRBuildRules::SnapInFrame`): walls, floors, furniture and
  neighbouring foundations line up with it, and the server accepts them (`IsSnappedIn`) with the same corner rules.
- **Ghosts have real shapes.** Stations, the Hearth, the totem and the chest show their real models and structure pieces
  their real planks, in the tint. Demolish's red copy of a station is its real shape too, not a box.
- **Reach counts from you, not the camera.** Looking past it, or up, a piece that goes where you aim comes down on the
  ground at the end of your reach and keeps following your look. With no ground under it (out over a drop) it's refused:
  a chest could be placed hanging in the air.
- **Tests:** `Tools/aimtest.ps1`, `Tools/buildaimtest.ps1` (in `alltests`), `Wildreach.Combat.LeadPoint`, more in
  `Wildreach.Building.Rules`. New commands: `WRLockOn`, `WRSnap`, `WRGhostInfo`, `WRGhostTurn`.

## 2026-11-06: version 0.6.0, a living island

Everything from Raven's Oct 31 review ships together: the creature pass, beds and waking on the map, worlds, world
settings and roaming wildlife (their sections follow). **Released 2026-11-06 as
[v0.6.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB, notes
`Docs/release-notes/v0.6.0.md`): installed copies update themselves on their next start. These are the release checks and
what they turned up.

- **Death on the real island offered nowhere to wake, and Wake did nothing.** Found by playing the packaged game on a copy
  of Raven's save: the bot stayed dead for over 80 s. `WRBeds::CampsFor` misread `TSet::Add`'s second argument (it says
  whether the name was already there), so it dropped every camp on the island. Fixed, and Wake now brings you back at
  your start when there are no places at all, so nobody can be stuck dead. Every test had passed because the arena's
  starts share one name and the island preview used made-up places; `Tools/worldstest.ps1` now dies on the real island.
- **The menu's island has no roaming packs.** It's no world and saves nothing, so `WRWildlife::Start` leaves it alone. The
  map's own always-out pack still falls behind the menu until a game starts (`RESCUE` lines in the log, none in play).
  Older than 0.6 and nobody can see it, so it stays for now.
- **An old engine warning, not new.** One `Ensure condition failed ... bPSOPrecacheCalled` from `ULandscapeComponent` in
  the log on the frame you click Play, in the menu's world, which is thrown away the next frame. It costs about 0.25 s
  there and shows nothing. 0.5.0 does the same after the same wait at the menu.
- **The old save carries over** (run on a copy, the real one untouched): the first Host copied it into Island, listed as
  Played 28 Oct, with Raven's level 12 character on the Fire path and all 9 pieces. Their real bed could be set, was
  offered on the death map with three camps, and woke them 13 s after dying. Back at the list it read "Played today, 1
  min played". A new world from the same list started a fresh character.
- **Full suite on the final code:** 28 of 30, and the other two passed when run again alone. `mp` counted 98 movement
  corrections where it allows 90, as the first test while other work was running on the machine, and 69 alone (about
  what it has always been). `walk` now has no roaming packs: it checks the ground, and a Magmaw had caught the unarmed
  walker 1 km out and sent it back to a camp. That's the game working, but it failed the test by chance.
- **Real Windows mouse and keyboard** on the packaged build with no test flags, on a copy of the save. All of these worked,
  with nothing wrong in the log: hover and clicks on the menus and world list; the New world form (typing, preset tiles, a
  toggle, `+`, a dragged slider); walking, the camera, an attack, the ability wheel and the hotbar keys; items dragged in
  the bag and onto the hotbar; the journal's tabs; Esc > World with a dragged slider and Back to the preset; the build bar;
  the console; setting a bed with E; waking at a camp clicked on the map and at the bed clicked on the list; Main menu; a
  new world; Quit. Block on the right button was pressed but not confirmed, as no picture or log line shows it.
- **Installer:** `make_installer.ps1` now refuses a build that has been run, since it zips the whole folder and saves and
  logs would ship. Run tests from a copy. `package.ps1 -SkipEditorBuild` cooks with the editor as built, because
  UnrealBuildTool won't build the editor while another project's editor or `-game` run holds a Live Coding session.
- **Benchmark, quiet machine:** Epic 8.38 / 8.35 ms (119-120 fps; 0.5.0 was 8.34), Medium 5.44 / 5.56 ms, one 141 ms hitch
  in one Medium run. Server 2.37 ms as usual (29 animals in 8 packs; was 2.16), 8.35 ms with every pack out (432 in 156;
  was 6.46 with 328). With packs brought out at 600 m as a stress test: 10.9 ms (92 fps), 188 animals, the worst hitch
  1,073 ms at the start while they all load. No tuning changes.

## 0.6.0: wildlife that roams

From Raven's Oct 31 review: always a small pack standing still.

- **Packs live on when nobody is near** (`WRWildlife`). Each is a record that keeps its place, what it's doing, how many
  are left and how hurt they are. The server moves them every 20 s over a coarse map of where animals can walk
  (`Data/island_walk.json`, from `Tools/terrain/walkmap.mjs`: 32 m cells, the sea, steep ground, lava, arenas and caves).
- **You meet them where they've got to.** A player within 250 m brings a pack out at its record's position, doing what it
  was doing; when everyone has gone it's a record again. A pack in view keeps walking at its own pace with its creatures
  following it, and waits for any caught up in a fight, a hunt, a meal or running away.
- **By kind:** grazers drift between feeding grounds and rest when they sleep; predators keep a territory and go after the
  nearest prey they can reach when hungry, faster, by night unless they sleep through it; a few packs travel far
  (`wanderChance` in `creatures.json`: Basaltbacks, Woolly drudges, Aurora foxes, Scorchwings, some Emberhorns and
  Frostantlers); burrowers and colonies stay home. Everyone keeps to their side of the island and their elevation band,
  and clear of the camps, as the sites always did, so the strong ones don't walk into the starting areas.
- **Hunts nobody sees are settled by odds** (`huntChance`, on the size rule the creatures already hunt by): caught or
  missed, the prey runs. Tames are never touched.
- **Pack sizes vary**, from a lone animal or a pair to herds of 12 (`pack` in `creatures.json`), mostly small. About 470
  animals on the island, up from 328.
- **A death is a death:** a pack no longer replaces its dead a couple of minutes later. Numbers and wounds stay with the
  pack, wounds mend slowly, and numbers come back slowly (10% of a kind's share an hour) by breeding and by new packs
  wandering in out of sight, towards its share: its sites x its usual pack x the world's creature numbers.
- **Creature numbers** now means how many packs: more or fewer at the start of a new world, and a change later comes on
  slowly.
- **Saved with the world** (`wildlife.json`, each minute with the buildings). Without a `world.json` a world starts over,
  and so does its wildlife. A world from before this starts new packs at the sites.
- **A pack never comes out inside a land claim:** it comes out at the edge of it, and doesn't pick a goal inside one.
- **Map-placed packs and bosses** hold their place and replace their dead as before.
- **Tests:** `Tools/roamtest.ps1` (in `alltests`): a player stands still through two fast island days while packs travel,
  rest, hunt and breed around them, then a herd is watched walking at normal speed, a pack is thinned and hurt and stays
  so through a server restart, and one inside a claim comes out at its edge. `Wildreach.World.WalkMap`. Moving them all
  costs about 2 ms each step. New commands: `WRPacks`, `WRHurtPack`, `WRFaceHerd`.

## 0.6.0: world settings

From Raven's Oct 31 review: four presets existed, but only a command-line switch could pick one.

- **The New world form has the rules:** pick a preset on the left and tweak it on the right. Players (PvP, PvP in boss
  arenas, what happens to what you carry when you die, offline raid protection, clan size), Speeds (XP, gathering, taming,
  crops) and The world (day length, hunger and thirst, creature numbers, creature difficulty, building upkeep, boss
  respawn). Sliders with - and + for one step. Picking a preset sets them all to its values.
- **The host can change them while playing:** Esc > World (`SWRWorldPage`, only on the host's menu): the preset, the same
  rows, and Back to the preset. Changes are saved and in force a moment after the last one, go to everyone in the game, and
  other players get a notice when the host is done.
- **Saved with the world:** `settings.json` holds just the preset, or `"custom"` with the base and only what differs from
  it. A changed world shows **Custom** on the list. `FWRWorldSettings`, `WRWorlds::PresetSettings / ReadSettings /
  WriteSettings`; `AWRGameMode::ChangeWorldSettings`.
- **Four new settings reach the game:** `server.dayMinutes` (the clock keeps its time when it changes),
  `server.hungerMultiplier`, `server.creatureNumbers` (under 1x, that share of the wildlife sites; over 1x, bigger packs;
  from the next start of the world, until roaming wildlife replaces sites) and `server.creatureDifficulty` (health and
  damage of wild creatures and bosses as they appear; tames keep their own numbers).
- **Boss respawn** is read when a boss dies, so changing it in game counts from its next defeat.
- **Not on the screen, because nothing is behind them:** crafting speed (crafting is instant), bounties and raids (no such
  systems yet). Their preset values are still in `Data/server_presets.json`. The preset descriptions on the form no longer
  promise them.
- **A client now takes the host's settings whole** (`ImportServerSettings` clears its `server.*` first), so a setting put
  back to its default doesn't linger.
- **Tests:** `Tools/rulestest.ps1` (in `alltests`) plays the island three times with different `settings.json` and compares
  the clock, hunger and thirst, sites and creatures, creature and boss numbers. `worldstest` now also clicks the form's and
  Esc > World's steps, with a friend in the game who gets the new rules and a notice, and whose Esc menu has no World
  page (28 checks). New commands: `WRWheel` (the mouse wheel over a piece of text), `WRVitals`; `WRSpawn` logs what it made.

## 0.6.0: worlds

From Raven's Oct 31 review: only one world save, and characters, clans and settings shared by everything.

- **Each world is a folder**, `Saved/Worlds/<world>/` (`WRWorlds`): `info.json` (name, created, last played, play time,
  version), `settings.json` (what `ServerDB/server.json` was), `world.json` (buildings, tames, clock), `clans.json` and
  `players/`. Characters are per world: everyone starts fresh in a new one.
- **Host a game opens Choose a world** (`WRUIWorlds`): the worlds on this PC, last played first, each with its preset and
  when and how long it was played. **Play** (or a double click), **Rename**, **Delete** (asks first: Keep it / Yes, delete
  it) and **New world**: a name and a preset (PvE first, PvP, Casual co-op, Playtest), then **Create and play**. The last
  world played is picked next time. Joining is unchanged. The full settings screen comes next.
- **The old save moves over by itself:** the first time the list opens, `Saved/ServerDB` (`world_L_Island.json`, every
  character, clans, `server.json`) is copied into a world called **Island**, with its preset kept, and left in place as
  the backup (`Saved/Worlds/moved_from_ServerDB.txt` records it). Hosting without a world named does the same for L_Island.
- **The game picks its world at the start of each game** (`AWRGameMode::InitGame`): `?World=<id>` from the menu,
  `-wrworld=<id>` for tests and dedicated servers, else the map's name without `L_`. It reloads the settings and clans
  then, so a second world hosted in the same run gets its own.
- **The menu's island is no world any more:** it used to load and save `world_L_Island.json` behind the menu, and left a
  character file for every machine-named player. Now it loads and saves nothing (unless a test gives it `-wrworld`).
- **Worlds a server was only pointed at stay off the list** (no `info.json`): a dedicated server's, and all the tests'.
- **Tests:** `Tools/worldstest.ps1` (in `alltests`): one game window, by real clicks, through two worlds and back:
  separate buildings, characters and presets, rename, delete, play time, and the old save copied into Island. Every test
  script now keeps its own world folder; `survivaltest`, `housetest` and `skytest` get their own instead of sharing
  L_Island's. Bot commands take `@2:` (only in the second world this run enters), for runs that pass through the menu.
- **`uiflickertest` no longer sees flashes that aren't there:** it split its frames by the log's own frame counter, which
  can change part-way through a burst of lines, so one look at the Skills tab (136 lines) came out as two jumbled ones and
  failed. `WRUIText` now starts each look with a `UIFRAME` line.

## 0.6.0: beds and respawn

From Raven's Oct 31 review: a bed should be where you wake, and still be there after a restart.

- **Beds are saved** with the world (`sleepers` on each bed in `world_<name>.json`), and your chosen bed with you
  (`bed` in your player save), so a restart keeps both.
- **Placing a bed or bedroll sets it** ("Built bed. You'll wake here"); using one sets it again.
- **Choose where to wake on the map** (Raven: "you die and then you're looking at the map"): the death screen is the
  island map with every place you can wake on it (your beds, your clan's beds, your path's camps) and a red pin where
  you fell, framed around them; click one to pick it (a brass halo), or pick from the list beside it, which gives each
  one's distance and direction. Then press **Wake** ("Wake at your bed"): it works once the 10 s wait is over and you
  wake there, standing on top of the bed. No timer wakes you any more (`player.autoWakeSeconds` > 0 brings that back).
  The default pick is the bed you last set (or another of yours that's ready, else your camp). Names on the map step
  aside for each other; the map zooms up to 12x for beds close together. `WRFlow preview death` shows it with made-up
  beds and that side's camps.
- **Beds rest, bedrolls wear out**: a bed can't be woken at again for 60 s after you use it ("resting, N s" in the list);
  a bedroll is used up when you wake on it ("Your bedroll is worn out"). Both are in `Data/tuning.json` (`beds`).
- **Beds on the map**: a new legend row, "Beds · where you can wake", with your beds in brass and your clan's in frost.
- New `WRBeds.h/.cpp` holds it all; a bed that's destroyed drops out of everyone's list.
- `Tools/flowtest.ps1` has a second phase: restart the server, check the bed was saved, place a bedroll, die, click
  "Your bed" on the death screen's map, check nobody wakes until Wake is clicked, wake on the bed, then die again: the
  bed is resting, so you wake on the bedroll, which wears out (22 checks).
  Test bots can act while dead with a `dead:` prefix (`dead:WRClickMap Your bed`, `dead:WRClick Wake at`); otherwise a
  dead bot presses Wake as soon as it can. New `WRClickMap <name>` clicks a named place on whichever map is up.
- The server gives Wake a second's grace: its world clock runs a little behind the death screen's countdown.

## 0.6.0: better-looking creatures

Raven asked for better, more realistic creature movement and looks, keeping the design doc's fantasy, and reviewed
every creature from front and side pictures; this is the work so far (round 2 of their notes).

- **New creature animation** (`Tools/art/anim_creature.py`): feet planted while they bear weight (two-bone IK), the body
  rising, rolling and swaying with its legs, a steady head, a tail on springs, attacks that wind up and strike in a tenth
  of a second; gaits picked by size and speed (walk, trot or amble, gallop). On Cinderfang, Woolly drudge, Ashstrider,
  Frostantler, Aurora fox, Emberhorn, Basaltback and Rimeclaw. Front legs stand as straight columns under the shoulders
  and fold at the wrist (Raven: "an extra joint" on Cinderfang's and Ashstrider's front legs, Frostantler's "bend towards
  the back feet"); the back stays near level.
- **Four legs** unless the real animal has more: Woolly drudge and Cinderfang lost their middle pair (the Kilnmite keeps
  six). Cinderfang's front right leg, what was left of the sixth, is now a mirror of the front left.
- **Shapes** (`rig_quadruped.py`: `--mirror-leg`, `--straighten-front`, `--legs-longer/--legs-thicker`,
  `--head-forward`, `--widen`): Woolly drudge on longer, thicker legs (its chest no longer meets them); Basaltback's head
  out from under its shell; Glacimaw 40% longer (`rig_special.py --stretch`) and far bigger (`halfHeight` 95, reach 380);
  Rimeshell wider (`meshWiden` 1.3, new in `creatures.json`).
- **Surface detail** (`Tools/art/bake_detail.py`, a normal map and shaded colour baked on each model's UVs): scales all
  over Magmaw, feathers on Snowpuff, fur on the Aurora fox, whose hard green patches are now pale fur under a soft
  teal-to-violet aurora. `M_CreatureGlow` takes a normal map (`MI_<Name>Skin` for creatures with no glow).
- **Ember glow** that really glows (`M_CreatureGlow`) on Magmaw, Cinderfang, Basaltback and Emberhorn; the red attack
  wind-up glow is off (`telegraph.glow` 0: the pose and growl warn).
- **Gloomhowl looks ahead** instead of at the ground: `poseLift` in `creatures.json` turns named bones nose-up over every
  clip (its pack clips hang the head low).
- **Gloomhowl's eyes glow at night** (Raven's pick): its red irises (the Barghest's own glow map) light up as the
  daylight goes, out again at dawn and when it dies. New `nightGlow` in `creatures.json`: the creature's materials'
  `GlowStrength` rises to it with the dark (`1 - daylight`); `MI_Gloomhowl` keeps the iris's own colour.
- **Woolly drudge walks on pillars**: its front shoulders sat 15 cm behind its feet, so each step reached far ahead on a
  slant and the hoof curled under in the swing. Re-rigged with the shoulder over the foot and the front joint up inside
  the body (`rig_quadruped --front-elbow 0.42 --front-hip-x 0.12`, clips `--swing-foot level`); the narrow neck every
  leg had above its hoof (a wine-glass stem once the leg stood upright) filled out to the hoof's width.
- **Woolly drudge is a giant** (Raven: 3x): 5.2 m to the hump (`halfHeight` 258, `radius` 270), reach 420 (its face is
  ~2.6 m ahead of its middle); its stomp ring grows with it (9.6 m). The pale and grey fur tips round its ankles and the
  tip of its beard now take the brown of the fur around them (tusks and horns stay ivory).
- `creatureportraits.ps1 -Frame <sizes>`: a wider camera for giants (the frame's fixed margin shrinks beside them).
- **Frostantler's antlers end in tines** (Raven): each main beam ended in a fat club with a holed knob (a melted blob,
  an S-shaped lump from the side); both are now tall pointed tines, and the dark flecks in the antlers are ice. It is
  **twice the size** too: 3 m to the antler tips (`halfHeight` 150, `radius` 110), reach 300. The pale cyan stripe down
  each flank fades into its body, and the nubs hanging under its belly (a stray piece in the game) are gone.
- **Rimeshell loses its side fin**: the image model gave it a broad flat fin sticking out of its left side under the
  shell (not in its concept), which also put its body ~13 cm off-centre in the game. The fin now folds into a lip under
  the rim (mirrored from the clean side), the body sits in the middle, and the front shoulders sit over the feet
  (`rig_quadruped --front-hip-x 0.34`: its steps reached ahead on a slant).
- **Snowpuff's toes are bird-sized**: the image model gave its feet toes about twice as long as its legs are tall, which
  scissored along the ground like dark planks as it walked. They are now about as long as its shins, so it steps like a
  fowl.
- **Aurora fox stands and trots, with a real fox's brush**: its tail was a ball as wide as its hips resting on the
  ground, so the old rig took it for a foot and mixed up its legs: it crouched with its rump down and crawled. The tail
  is remade as a long brush that leaves the rump nearly level and droops in an arc, bushiest past the middle and closing
  to a point (its old surface and colours swept onto the new shape; where it had grown into the backs of the thighs
  it is cut free), and covered in 3200 fur locks combed toward the tip. Re-rigged on its four real legs with the
  shoulders over the paws, on the new animation (`anim_creature --build light --attack bite`).
- **`hair_locks.py --along`**: locks lie along a line in the side view instead of one way (a tail's fur combed toward
  its tip round the curve).
- **Creature films save a tenth of the frames** (Raven: "do you need to take hundreds of screenshots of the same
  frame?"): `WRRecord` takes windows of frames to keep (`first-last/step,...`; every frame's time is still logged), and
  `creaturevideo.ps1` keeps 88 of the 756 by default (`-Keep all` for every frame): the idle once a second, the walk
  and run every half second with a steady stretch of each at every 2nd frame (f126-161, f234-257), the bites every 4th,
  the ramp, flinches and death every 20th. A film is ~90 MB instead of ~1 GB and records in 30 s instead of ~170 s.
- **Vulkarn's right side is whole again**: the boss build cut out the faces where its right arm had been fused to its
  body, which left a hole down its flank under the armpit (the floor showed through it in every swing), and the skin
  round it, partly weighted to the arm, tore into strips that hung off its back. The hole is closed on the shape of its
  left flank (mirrored), in its skin colour, and the skin round it takes the left side's weights, mirrored
  (`WildreachArt/mixamo/bosses_r2/vulkarn/vk_fix.py` on the game model; rig and clips unchanged). And he's bigger
  (Raven): 6.5 m instead of 5.2 (halfHeight 325, radius 188; his attacks reach from his body's edge, so they grow too).
  His two small holes (a hand, a thigh) are really closed now: `bmesh.ops.holes_fill` had made nothing on them, so a
  face is built from each hole's corners in order. The hand's patch faced inward (the sky still showed through it from
  outside), so both small patches have a back side too.
- **The Obsidian Warden's holes closed, and it's 20% bigger** (Raven): the same boss build left holes under its left
  armpit and on the front of its left shoulder (where the image model's left arm met its body), and a few round its
  neck; the sky showed through its shoulder in some swings. Each is filled with one patch in the skin's colour, with a
  back side (`WildreachArt/mixamo/bosses_r2/obsidian_warden/ow_fix.py` on the game model; rig and clips unchanged).
  Nothing round them tears in any clip, and its walk, run, swings and death play clean. A check that looks through the
  model from 26 directions in four clips (`WildreachArt/mixamo/bosses_r2/seethru.py`) finds nothing see-through on
  Vulkarn and only two pinhead specks on the Warden in one idle; the light patch in its shadow is the sun through the
  gap between its bent arm and its body. halfHeight 204, radius 132 (was 170, 110).
- **Ignessa: a face, real hair, wings on her back, arms and legs that move** (Raven: hair "like actual hair", wings
  joined "by her shoulder blades", "her face looks messed up"). Her face was a blur of the concept's (dark rings for
  eyes, the mouth a round open O): the concept's face is now laid onto it from the front, lined up on the eyes and mouth
  and blended into her own skin. Her hair is 4500 thin locks over the old lump (`Tools/art/hair_locks.py`, which can
  now keep to one material: the built wings share its UV space). Her wings hung off the back of her hair at neck
  height; they now grow out of her back opposite her breasts (Raven: at the shoulders they still looked joined at her
  head), where her chest stands farthest forward (`--wing-z` to set it). She was one rigid piece (a body bone, a head, wings):
  `rig_special.py --body fae --wing` now gives her thighs, shins, feet, upper arms, forearms and hands: legs hang soft
  with pointed toes and trail behind her in flight, both hands draw back and thrust at the target in her attack, arms
  fling out as she falls. Her upright body bone had no roll, so every lean (the lunge, the flight, the fall) tipped her
  sideways: it leans forward and back now. Her flame trousers were flat boards (16 x 11 cm, hip to ankle): deepened to
  round legs, and the concept's white background flecks on her legs and hands painted out
  (`WildreachArt/out/ignessa_v3/ig_fix.py`). Then her head "looked like a mess" (Raven): locks grew out of her crown
  and hung over her forehead, and the crown (a fan standing up from her hairline) was painted with hair streaks and
  white blobs. The crown is one fiery piece now, ember red to flame-yellow tips, and hair grows only behind it, at her
  temples and down her back. Then the crown was "just a flat piece on her head" (Raven): the image model stood it up as
  a flat fan in front of her head (2 cm thick). It is bent round her head into a circlet now, from her brow back to her
  temples, its outer spikes at her sides; the hair on top grows from just behind its ring. And its colours match her
  wings so it stands out of her red hair (Raven): the wing picture's orange at its foot through gold to pale cream at
  the tips, with dark vein lines up its lower half. Size unchanged (halfHeight 110: 2.2 m to the crown).
- **Grimjaw's holes closed**: the same boss build left seven holes (both shoulders, under the left arm, one at his
  hip strap); the floor showed through his shoulder in every step of his walk, and the see-through check found 110 to
  160 spots per clip. Each is filled with a patch and a back side (`ow_fix.py --around 3`: his holes' edges are darker
  than his skin, so the patch takes the skin a few edges out from the hole; with the edge's own colour they came out
  as dark blue blotches). The check finds nothing see-through on him now. The dark shape that crosses his back in his
  swipe is his own raised arm's shadow. Walk, run, swipes, jump attack and death play clean.
- **Grimjaw 30% bigger, and his back cleaned up** (Raven: "his rags/back area is weird"): halfHeight 208, radius 130
  (was 160, 100). The image model made his seat and the back of his fur loincloth one boxy block, its top edge a ledge
  jutting 25 cm past his lower back: it is drawn in to a rounded rump that tapers to his thighs, and its lower half
  partly follows his thighs so it swings with his legs. What looked like dark metal plates on his flanks and shoulder
  were faces left by the boss build's cut with their UVs stretched across a strip of the texture; every face stretched
  like that (245) takes its neighbour's colour. The pale, whitish tatters at the loincloth's hem take the fur's brown
  (the concept's fur is dark to its tips). `WildreachArt/mixamo/bosses_r2/grimjaw/gj_fix.py` on the hole-filled
  model; rig and clips unchanged, nothing see-through.
- **Grimjaw's fur is real fur, his skin one blue, the dark patch on his back mostly gone** (Raven: "the dark patch on
  his back looks off and his rags still don't look right"). His fur pieces (loincloth, forearm cuffs, shin wraps)
  were faceted brown lumps with fur painted on: 6000 brown locks grow on them now (`hair_locks.py --mixamo`, which
  exports the way the Mixamo bosses are built). His skin came in two blues, pale over his shoulders and upper back and
  dark below with a torn edge between (a ragged vest): both are moved to their middle, each keeping its own detail.
  The dark oval on his back while he punched or swiped was a shadow (it goes with shadows off) from a ridge his
  shoulder skin bunched into. The cause: the rig gave his HEAD bone the skin of his trapezius and upper back (up to 94%,
  64 cm out from his head), so every move of his head dragged his back and shoulders into creases and puckers. Outside
  his head's own outline that hold now goes to his shoulders, neck and upper spine, the shoulder weights are smoothed,
  and his upper back and shoulders are smoothed in shape and paint (Raven circled them: "can we smooth this section
  out"). The oval is gone and his back reads as smooth muscle. His hole patches are now domed on the skin round them
  (`ow_fix.py --dome`) instead of one flat face each, and the shadow painted in his armpits is gone.
- **Vulkarn and the Obsidian Warden checked for Grimjaw's faults** (Raven): neither rig ties back or shoulder skin to
  the head (Vulkarn's head bone holds no skin; the Warden's only its head). Both had faces with their UVs stretched
  across a strip of the texture: 127 on Vulkarn, 509 on the Warden (on its shoulder plates, chest and back), which
  showed as faint smears and amber flecks on its dark armour. Each takes its neighbour's texel
  (`WildreachArt/mixamo/bosses_r2/stretch_fix.py`); rig, shape and clips unchanged, both re-filmed clean.
- **The Pale Hunter: whiskers off, re-rigged, a real bite**: its whiskers were a spray of thin wire loops off its
  muzzle (grey plates either side of its mouth in the game): cut out and the roots closed
  (`WildreachArt/out/pale_hunter_v2/ph_fix.py`; small flat stubs are left at the mouth corners). It ran on its October
  rig, whose bite was a slight dip of the head and whose new walk folded the front paws up: re-rigged with the front
  joint high inside the body and the shoulders over the paws (`rig_quadruped.py --head +x --front-elbow 0.4
  --front-hip-x 0.32`), clips from `anim_creature.py --build light --attack bite` (a trot, a gallop, a head strike).
  `anim_creature.py` could not find a boss (the `_bosses` divider in creatures.json has no id): fixed. What looked like a
  see-through sheet under its belly is the pale blue paint of a fold of its skin, so the shape is kept. Size unchanged
  (halfHeight 90).
- **Leftovers tidied** (Raven): the Pale Hunter's flat whisker stubs at the mouth corners and the tufts over its eyes
  are smoothed back into its face (`pale_hunter_v2/ph_smooth.py`; cutting them harder opened dark holes); the notch at
  the back of Grimjaw's neck is smoothed with his back (his nape is in the smoothing, his beard and throat kept); the
  Obsidian Warden's orange cracks glow at night (`MI_Obsidian_wardenGlow`, fire glow 7: they were only painted); the
  stray lock that stood out at each of Ignessa's temples is gone. The Aurora fox's tail was already remade as a brush
  on Nov 3 and is left as it is.
- **The food chain at the new sizes** (Raven: keep the size rule; a hand-drawn web of who-eats-whom was too
  confusing): checked once every creature's size was set. Nothing hunts the two herd giants (Basaltback, Woolly
  drudge), every hunter has prey on its own side, and packs still take an Emberhorn. One pairing was wrong: the small
  Scorchwing could take a 2 m Ashstrider (exactly at its limit). A lone hunter's prey limit is now 1.25 times its own
  width (was 1.3; `creatureAI.preySizeRatio`), which removes that pairing and changes nothing else; packs keep 2.
- **Skaldrith, twice the size, on its feet on slopes, in frost colours** (Raven): halfHeight 360, radius 340 (its back
  about twice the survivor's height). Its body stayed level on a slope with its feet in the air or in the ground (the
  pack dragon's skeleton wasn't one the feet system knew): `creatures.json` can now name a pack skeleton's legs
  (`"legs"`), and Skaldrith's body tilts to the ground with each foot planted on it. Its faint blue tint showed as plain
  grey in the game sun: now ice blue with frost white on its lighter parts (`M_CreatureTint` takes a second, lighter
  tint). Scorchwing, the same pack dragon, names the same legs (Raven): its body tilts on slopes too.
- **Bosses can be filmed**: a boss's brain is a plain AI controller, so the film commands never reached it (Vulkarn stood
  still and "couldn't bite"). Bosses now run the test controls themselves (walk, turn, stand; no fighting meanwhile),
  and `WRCreatureAttack` swings a boss's next attack clip where it stands (no telegraph, no hit).
- **Glowmoth's wings beat up and down together** (they beat in opposition: a mirrored-bone sign).
- **Four rebuilt from new concepts** (Raven's picks): **Rimeclaw** (concept 3 with concept 0's curled tail; the face
  clear of the mane, on the new animation); **Ignessa** with her arms and legs apart (her body cut out of concept 3)
  and wings built as separate pieces from one wing picture (`rig_special.py --wing`), hinged behind the shoulders and
  moving only with the wing bones; **Vulkarn** (concepts 1 and 3: lava cracks, beard, hood) and **Grimjaw** (concept
  1, no horns, hands clear of the legs), both re-rigged on Mixamo with fresh Creature Pack clips.
- **Boss rigs at the right height**: the Mixamo copy was always packed as a 1.8 m figure while the bosses' full models
  are 2 m, so every joint sat 10% low (shoulders sunk into the chest, Raven's "mixed up near the shoulders").
  `pack_for_page.py` takes the model's height now.
- **Cinderfang rebuilt from concept 1** (Raven's pick: flame-lit face and horns, mane, raised tail). The old model came
  from a six-legged concept and had five paws, front legs patched from a leftover leg. Five shapes from different
  generator seeds, paws counted on each (all four clean); seed 42 kept for its horns and straight front legs. Torso
  stretched 1.2 (three-quarter concept), `meshScale` 1.08 so the raised tail doesn't shrink it (the game fits the whole
  height to the capsule). Then, on Raven's notes ("tail not attached to its rear correctly", "front legs still messed
  up", "his leg is disconnected"), `cf_fix.py` (in WildreachArt) on the rigged mesh: the tail had come out as a flat
  sheet down the outside of the left thigh, so it is cut off and a new one grown from the top of the rump (a tapering
  core under 130 flat locks like the mane's, arcing up and back to a curled tip); the right front leg reached forward
  and zig-zagged when stood up, so the body is made symmetric from its left side (upright leg, one connected surface;
  a mirrored copy had shown its seam at the shoulder) and both front legs lean in under the chest. Then ("its front
  legs have an extra joint"): the rig had folded them at 0.16, partway up the forearm above the real wrist, so it is
  re-rigged with `--front-wrist 0.11` (the top of the paw), and a knob halfway down the back of each leg is smoothed
  away; they stand and trot as straight columns, folding only at the paw. Then (Raven circled the paw hooking back
  in the swing: "have it be straight with the lower leg"): a long forearm on a short pastern can only shorten by
  folding the paw right back, so the front joint is now the elbow under the chest (`rig_quadruped.py --front-elbow`,
  apex back) and the forearm and paw swing forward as one straight piece, flat again to touch down.
- **Magmaw walks on two legs** (Raven's pick). Its model is built like a two-legged dinosaur (six new generator seeds
  from concept 2 came out the same): hind legs under the middle of the body, arms hanging just above the ground right in
  front of them, so four-legged walking crouched it (the old picture) and stretched-arm walking crowded the legs.
  `mg_fix.py --fold 55` (in WildreachArt) swings the arms up in front of the chest; rigged with `--legs 2 --hip-z 0.52`
  (its sail had read as the back), new planted-feet clips (it still had the first rig's), same texture and baked scales.
- **Scorchwing kept as it is** (Raven: small, and the pack's Mountain Dragon model, the Skaldrith boss's, tinted).
- **Basaltback cleaned up** (`bb_fix.py` in WildreachArt\out\basaltback_v2): its face (trunk, tusks) had come out turned
  0.1 to one side and is slid back onto the centre line; the side the concept doesn't show (a flat sheet for an ear, a
  loose flap on a front leg) is replaced by making the body symmetric from the good side; the head, painted pale
  blue-grey at 2.5x the legs' brightness, is recoloured to the legs' dark tone (the tusks warmed to ivory, the concept's
  glowing marks on the forehead kept, the shell untouched). Re-rigged with the front elbow, new heavy clips with a ram.
  Then **a giant walking tortoise** (Raven): 2x (halfHeight 70 -> 140, radius 140, reach 400: 280 cm to the shell top),
  legs 1.45x longer (`--legs-longer`), the head and ears moved forward as one piece on a longer neck (`bb_fix.py
  --neck 0.10`; the rig's `--head-forward` had dragged the shell's front along), the flaps hanging off the front legs'
  tops and the tusk that curved back toward the legs cut off. `rig_quadruped.py --belly-z` (that tusk had read as the
  belly). A 2x2 foundation platform on its back is parked: the 4 m grid needs an 8 x 8 m flat back.
  **Then rebuilt from a new concept** (Raven: "its ears now stretch back to its shell", "still looks like the ears are
  messed up": stretching that model into a neck came out boxy, its head was tucked under the shell). New concepts
  (`batch_r2_basalt.json`: an elephant-head set and a tortoise-head set); Raven picked elephant-head 1: an elephant on
  tall legs under a broad tortoise shell with three lava patches a side. Five generator seeds, seed 7 kept (four clean
  feet, roundest shell), the shell's top flattened a little (`flatten_top.py 0.4`), rigged with the front elbow, heavy
  clips with a ram; halfHeight 150, radius 150, reach 430 (300 cm tall). Sources in WildreachArt\out\basaltback_v3.
  **Building on its back** is to be its own feature later (Raven). Then ("make its tusks longer", "the front legs move
  weird just like Cinderfang's"): tusks 3x as long with the tips curving forward (`tusks.py`: the ivory texels, each tusk
  stretched along its own axis from the face), and the front joint moved up inside the body (`rig_quadruped.py
  --front-elbow 0.42`; at the belly it had bent the elephant legs backwards mid-leg): each front leg swings as one pillar.
- **Emberhorn cleaned up** (`eh_fix.py` in WildreachArt\out\emberhorn_v2): its left front leg had come out mid-step
  (lifted, bent back), so the body is made symmetric from the right side; the hide, painted mid-grey and rendering pale
  blue-grey, darkened to the concept's charcoal (orange markings and glowing horns kept); the front legs, slanting back
  from the shoulder to the hooves, sheared upright. Rigged with the front joint inside the chest (`--front-elbow 0.43`)
  and the shoulder set by hand (`rig_quadruped.py --front-hip-x 0.20`: fur hanging off the legs stopped the trace low and
  put the shoulders behind the hooves), so each front leg swings as one straight piece; `anim_creature.py
  --swing-foot level` keeps hooves hanging through the swing (following the leg they pointed straight ahead). It was
  still on its first rig's clips; new heavy clips with a charge.
- **Kilnmite repainted to its concept** (`km_fix.py` in WildreachArt\out\kilnmite_v2): the image model's paint had
  navy-purple legs and belly and a gold shell with baked highlights (it rendered as polished gold), and none of the
  concept's kiln cracks. Legs and belly are now a warm dark brown, the shell dark copper, and a crack network glows
  orange across the shell plates (the edges of 3D Voronoi cells at each texel's position, warped jagged and faded out in
  places, so cracks run on across texture seams and break into separate fissures); `MI_KilnmiteGlow` lights the cracks
  alone. Re-rigged on six legs with the current `rig_quadruped.py` (the old rig's weights were poor). `anim_creature.py`:
  six legs never gallop (they scurry in tripods at every speed; Kilnmite bounded like a dog) and step at most 1.2 leg
  lengths (long strides reached the front legs out like arms).
- **Ashstrider fixed to its concept** (`as_fix.py` in WildreachArt\out\ashstrider_v2): the image model had turned its
  head 28 degrees to its right (every walk carried it crooked), painted the face white with big black goggles round the
  eyes and a black ball of a nose (it read as a skull), and the body near-black between bright white lines. The head
  turns back straight about the top of the neck, the face is mid-grey with dark almond eyes and a dark grey nose, and the
  body's cells are mid-grey between pale lines like the concept's. Re-rigged with the current `rig_quadruped.py`
  (`--belly-z 0.44`: its legs stand mid-stride, so one crossed the centre line and read as the belly at 5% height;
  `--front-elbow 0.5`); new light clips (a trot, a gallop, a horn ram) with hooves hanging in the swing. Raven: "slightly
  bigger than Cinderfang", so 1.15x (halfHeight 85 -> 98, radius 45 -> 52): 196 cm to the horns, back at 145 cm against
  Cinderfang's 130, as long as Cinderfang with its tail.
- **Glowmoth fixed to its concept, and it glows** (`gm_fix.py` in WildreachArt\out\glowmoth_v2): the image model's
  antennae lay flat, straight out in front at mid-body height and as thick as fingers (from the side, two long sticks);
  they're thinner now and rise 35 degrees from the head. Its abdomen was striped black and yellow like a wasp's (the
  concept's is bronze plates over dark slate) and is now bronze and slate. The design's wings "glow soft blue": the
  dull blue eyespots are a clear cyan now and `MI_GlowmothGlow` lights them (a new MOTH glow in `creature_glow.py`,
  bluer and brighter than FROST asks so the pale blue fur stays dark), which shows best at night. The raised antennae
  are now its tallest point, so `meshScale` 0.35 -> 0.54 and `rig_special.py --hover` 2.5 -> 1.6 keep its 1.6 m
  wingspan and flying height. `creatureportraits.ps1` parks the hidden survivor past the 25 m nameplate range (a small
  creature's nameplate sat across it in the picture).
- **Pumicehog fixed to its concept** (`ph_fix.py` in WildreachArt\out\pumicehog_v2; the grey pumice rodent of
  `pumicehop_0.png`): the image model's head was turned 27 degrees to its right (from the three-quarter concept) and
  turns back straight, through a temporary armature heat-weighted to the mesh so the cheeks blend into the shoulders.
  Its front paws' claws touched in the middle (the rigger read them as one foot) and now stand a little apart, and a
  thin white spike above its right eye is gone. Its grey read white and its back yellow in the game's sun: the grey is
  darker and cooler with pumice pores, the back a deeper orange, and the design's "warm orange glow in the cracks" is
  cracks through the orange that `MI_PumicehogGlow` lights (FIRE 4). Re-rigged (`--front-elbow 0.3`: folding at a low
  wrist hooked the paws back) with new light clips; `anim_creature.py --gait hop` (new) makes it hop at a walk and bound
  at a run, as the design's hopping critter (its old clips hopped too).
- **Rimeclaw fixed to its concept** (`rc_fix.py` in WildreachArt\out\rimeclaw_v3; concept 3, the white ice lion): its
  whiskers were thin wire loops standing off the muzzle, which the game drew as flat grey plates either side of the mouth;
  they are cut out (thin both ways, at mouth height, so the mane's flakes stay) and their roots closed. The whole mane
  had come out blue: its front half is the concept's white ruff round the face now, with the blue kept along the crest,
  the back of the mane and the chest. Its dark slit eyes are the concept's glowing ice blue, the same size and set
  evenly (one had come out bigger and lower), with round pupils like a big cat's, lit by `MI_RimeclawGlow` through a
  new `EYES` glow kind in `creature_glow.py` (bluer than any fur, so the blue mane stays dark). Its dark navy lower legs
  (baked shading) are lifted toward the body's white. Re-rigged with an elbow under the chest (`--belly-z 0.40
  --front-elbow 0.36`: the rigger read the belly at 0.20) and new light clips. `rig_quadruped.py` now gives both hips of
  a pair the same place along the body (a leg modelled mid-stride put Rimeclaw's hind hips 14 cm apart), and
  `anim_creature.py --lift` (new) scales paw clearance in the swing: Rimeclaw walks at 0.6, since at the light build's
  full lift its forearms swung up level like a show horse's.
- **Rimeclaw's mane is hair now, and Rimeclaw is Emberhorn-sized** (Raven: the mane looked like part of the body, not
  hair; about as big as Emberhorn, a tad smaller). New `Tools/art/hair_locks.py` grows 2200 locks over the rigged mane
  (`rc_fix.py` now also saves the mane as a mask): tapered, flattened, combed out from the face and back, the crest
  back over the neck and the chest hair down, each drooping at the tip, each taking the colour (white ruff, blue crest,
  back and chest) and skin weights under its root, like the concept's. Locks rooted in the texture's dark creases are
  skipped. Rig and clips from WildreachArt\out\rimeclaw_v4. Size: halfHeight 60 -> 95 (190 cm to the top of the mane,
  about 2.5 m long; Emberhorn stands 210 cm and 2.7 m), radius 88, lunge reach 300; at that size its walk is a trot.
- **Glacimaw rebuilt behind the head** (Raven: the body should be longer, the spikes should look like spikes, the tail
  end looked like a blob). The old `--stretch 1.4` had stretched the whole model along its length, smearing the spikes
  into low fins, and the image model's tail curled under into a lump. `gl_build.py` (WildreachArt\out\glacimaw_v2)
  keeps the image model's head (jaws, teeth, horns; the yellow-brown teeth now ice white, rusty spots in the mouth
  gone) and grows a new body out of the neck's edge, after concept a: about 5.6 m of overlapping pale ice-blue plates
  (was 3.9 m), arching up behind the head, then lying along the ground and tapering to a point; a crystal spike on every
  plate down the back, raked back and tallest mid-body, and a fringe of smaller crystals along the lower flanks, all
  pale to white at the tips. The body is painted into the right half of a texture twice as wide. Glossy skin
  (`MI_GlacimawSkin`: `creature_glow.py` entries take a third, optional set of material scalars now).
  `rig_special.py` gives loose pieces automatic weights skip their nearest skin's weights. Then Raven doubled it:
  halfHeight 95 -> 198 (the crest stands about 4 m, its jaws about the survivor's height), about 11 m long, radius
  90 -> 180, spit range 380 -> 940 (its head sits about 5.6 m in front of its middle). A cave Glacimaw still fits the
  tunnels (5 m wide, 4.5 m high), but its tail reaches into the cave walls.
- **Glacimaw lies flat and moves like a snake** (Raven: no hunch at rest, slither like a snake, and bite by picking up
  its front half and chomping). `gl_build.py --flat` (output in WildreachArt\out\glacimaw_v3) lays the body straight
  along the ground from the neck, which eases down from the head's height; no arch. It also finds the lower jaw (the
  mouth is modelled a little open, the upper teeth hanging past the lower lip, so by distance over the surface from the
  chin versus the upper gums) and saves it for the rig. `rig_special.py --body worm --head-info` gives it a rigid head
  bone and a jaw bone hinged at the corner of the mouth, and new clips: it slithers in waves that run back down the body
  (the neck sways a third as much, the head keeps looking ahead), lies still at rest with a slow sway, a flicking tail
  tip and the jaw breathing, and bites by lifting its front half about 4 m up, jaw gaping, coiling back, then striking
  forward and down as the jaw snaps shut, with a shake, before lowering flat again; its spit uses the same strike.
  About 11 m long still (halfHeight 198 -> 120: lying flat it stands 2.4 m to its horns and spikes), bite range
  940 -> 640 (where its jaws land at the strike), walk and run paced to the waves (`pace` 315 / 480 cm/s), `strikeAt`
  0.53. New creatures.json `ownWindup`: an attack clip that rears on its own skips the telegraph's rear-back (tipping
  the whole worm back sank its tail into the ground).
- **Glacimaw lies along slopes** (Raven: tilt to follow slopes; on the ramp its 11 m body had stayed level, the tail
  floating and the head in the slope). Legless creatures with a spine chain (`rig_special.py` worms) now trace the ground
  under every joint, like the creature feet (same range, smoothing and fade), and each spine bone turns (up to
  `anim.creatureSpine.maxBend`, 35 degrees) so the body lies along the ground: up a slope, over a crest or bent at a
  slope's foot, whatever it is doing (a rearing bite rises from the ground under it). `creaturevideo.ps1 -RampCam 90`
  films the ramp climb side-on (default 110, from behind its right).
- **Glacimaw's body takes its head's colours** (Raven: the new body and the head didn't match or mix). The body had been a pale
  ice blue a quarter brighter than the head's steel blue, and smooth beside the head's mottling; in the game its gloss took
  the sky and it read nearly white. `gl_build.py` now paints the body from the head's own skin (its mean colour, with more
  colour: the head's grey horns and bare patches pull the mean grey), lighter on the back and darker underneath, with
  darker blotches across the plates like the head's, softer plate-edge highlights, and crystals that go from the body's
  blue at the root to a bluer pale ice at the tips (were near white). A softer sheen too (`MI_GlacimawSkin` roughness
  0.45 -> 0.6, specular 0.45 -> 0.35). The head piece's neck takes the same colours, so the join blends.
- **Big creatures' bites reach.** A creature's bite swept only the weapon reach (220 cm) from its middle, whatever its
  size, while the AI stops to bite at its `attackRange`: anything over about 2.6 m (Glacimaw, Basaltback, Emberhorn,
  Magmaw, Cinderfang, Rimeclaw) bit at the air. The sweep now reaches the creature's `attackRange` when that's longer.
  A spit leaves from the creature's jaw (or head) bone when its art has one, not the front of its capsule (a long
  Glacimaw spat from the middle of its body).
- **Sizes against the survivor (180 cm)**, Raven's picks from side-by-side shots: **Magmaw doubled** (halfHeight 55 ->
  110, radius 90, bite reach 340: 220 cm to the sail tip, head level with the survivor's) and **Cinderfang 1.75x**,
  horse-sized (halfHeight 45 -> 79, radius 66, reach 320: head at the survivor's shoulder, about 1.9 m long), and
  **Emberhorn 1.75x**, a mount bigger than its rider (halfHeight and radius 60 -> 105, charge reach 240 -> 360: 210 cm to
  the hump), all re-animated at the new size (slower, longer strides; at the same 480 cm/s an Emberhorn's run is now
  a heavy amble, not a gallop). `anim_creature.py` now counts `meshScale` in a creature's size
  (Cinderfang's strides had been planned 8% short). Between those and the giant Basaltback, Cinderfang packs and Magmaw
  counted as big enough to hunt a Basaltback (prey up to 2x / 1.3x the hunter's radius) and `Wildreach.Creatures.Behaviour`
  "nothing hunts a Basaltback" failed; at radius 150 it is out of reach of both again. Raven will settle the food chain
  once every creature's size is set.
- **A stomp's ring grows with the body** (`creatureAI.stomp.bodyRadius` 90: the 320 ring is a Woolly drudge's, a bigger
  body stomps one as much wider; the giant Basaltback's is 5.3 m). At 3.2 m the Basaltback bit from further out than
  its stomp reached and never stomped (`Tools\aitest.ps1` "a Basaltback stomps" failed).
- **Rig fixes** (`rig_quadruped.py`): a hip traced across the middle takes its partner's, mirrored (Cinderfang's left
  front shoulder had landed on the right of its chest); the belly is measured on the centre line (front legs standing
  close together had read as belly and dropped the hips); automatic weights go on the biggest piece alone and loose
  pieces (fur strands, a separate tail) take their two nearest bones (130 strands had made the weights fail outright,
  leaving the whole body on rough envelopes); a shoulder or hip carries on along the slant of the leg's middle, not
  its top few points (they swing toward the chest the leg merges into: Cinderfang's shoulders had landed in front of
  its legs, tilting them forward when standing); `--front-wrist` sets the front fold's height; `--front-elbow` makes
  it an elbow; `--hip-z` sets the hip height (a sail or crest reads as the back). `anim_creature.py`: a front joint
  above half the shoulder's height is an elbow (bends back, no paw flick, the paw keeps its angle to the forearm through
  the swing); two-legged creatures work (body measured to the chest, legs step in turn at every speed, the body comes
  down level where a foot can't reach and the bite lunges half as far: Magmaw's first bite reared it upright). The
  front-leg middle-slant hip rule applies to front legs only (Magmaw's zig-zag hind legs put their hips in front).
- **Art camera** frames the creature's own model where its animation has it (it had been framing the empty character
  mesh, so a hovering moth sat off the top of the picture); `WRTuneSet <path> <value>` sets a tuning number locally.
  `Tools\creatureportraits.ps1 [ids]`: one picture per creature, front over side, on the 1 m checker stage. Bosses get
  a wider frame and the hidden survivor parks outside their aggro radius (they had been fighting it mid-shot).

## 0.6.0: creature feet on uneven ground, frame-exact creature videos

- **Feet on slopes and bumps** for the generated creatures (`Tools/art/rig_quadruped.py` skeletons: `<leg>_upper /
  _lower / _foot`, 4 or 6 legs; pack creatures and the Mixamo bosses are left alone). Near a local view the game thread
  traces the ground under every foot each frame (`AWRCreature::TickCreatureFeet`, eased); the animation proxy
  (`FWRAnimProxy::PlaceCreatureFeet`) tilts the body to the plane through those points (pitch front to back, roll side to
  side, clamped 30 / 15 degrees) about a point over the feet at hip height, sits it at the ground's height and sinks it
  so the lowest foot reaches, then two-bone IK puts each foot at the clip's own position plus its ground height (a
  stepping foot keeps its lift; the knee bends as the clip bends it; the foot lies to its ground). Off in the air, in the
  water and out of range (fades); once dead the legs let go and the body stays lying to the slope. The survivor's
  standing foot plant now shares the leg solver (`WRAnimDetail::SolveLeg`), unchanged. Numbers: tuning
  `anim.creatureFeet`; `wr.CreatureFeetLog 1` logs each ankle's height over its ground.
- **Packs out of step**: every creature starts its idle and stride at its own point (seeded by its spawn spot, so the
  same spawn replays the same; `anim.creatureRandomPhase`). Before this they all started at 0.
- **Creature videos** (`Tools\creaturevideo.ps1 -Creature <id>`): a standalone game on the new `L_CreatureStage`
  (`Tools\make_map.ps1 stage`: a 1 m checker floor beside a 20 degree ramp) with a fixed time step
  (`-UseFixedTimeStep -FPS=30 -FixedSeed`) records a scripted performance frame by frame: idle, walk, run, two bites,
  a turn, a walk up the ramp, two flinches, death (25.2 s, 756 frames), identical for every creature and run, plus a
  contact sheet and the world time of every frame. New dev commands: `WRRecord <prefix> <frames> [w h]` (a screenshot of
  every frame), `WRCreatureDrive / Face / Attack / Flinch / Free <id>` (a creature does as it's told, its brain off),
  `WRCreatureHP <id> 0` kills, `WRHideMe 1`; the art camera keeps out of the ground.

## 2026-10-28: version 0.5.0, the feedback pass

Raven played 0.4.1 and sent a list. Five agents took it in their own worktrees (world, water, gather, combat, ui) and
the lead merged them; Raven approved the defaults below.

- **Lava** (world). The "lava squares" were scattered 6 m cells drawn as flat quads (edges floating on slopes) in a
  glossy emissive the day exposure washed to pale pink. `Tools/terrain/dressing.mjs` now traces the river's real channel
  into a 2 m grid (`hazards.lavaGrid`: ground height and depth inside the edge), and `AWRDressing::BuildLavaFlow` lays a
  sheet on the landscape's own vertices whose rim sinks under the ground (38 chunks). `M_Lava` is rebuilt: dark crust,
  glowing cracks, a molten middle drifting downstream (`lava.*`, live with `wr.LavaParam`). A spot burns only where the
  drawn surface stands over the ground (`FWRBiomeHazards::FLavaGrid::SurfaceAt`); trees and rocks stay out of it.
- **The green and yellow cones** (world) were the 188 placeholder resource nodes. `WRResourceLook.cpp` gives each item a
  look from packs already in use: charred trees and pines for wood, boulders, ore boulders with coloured lumps, a sulfur
  crystal cluster, lichen pads, berry bushes, fibre plants; the node's own mesh is a hidden collider (`world.nodeLooks 0`
  shows the cones).
- **Caves you can walk into** (world). The landscape filled the tunnels (each cave sat at the lowest ground under it, so
  the floor rose 2-3 m up the tunnel). The meshes were measured in Blender (`Tools/art/cave_measure.py` ->
  `Tools/terrain/cave_hollows.json`) and `dressing.mjs` fits each cave's height, variant, turn and size for at least
  3.6 m of headroom from outside to the back of the chamber; chambers are lit brighter and their ore can be swung at on
  uneven floors. `Tools/terrain/cavetest.ps1`: a bot walks into all six and mines.
- **Denser island** (world): shrubs 8,383 -> 24,435 (clumped, 1.7x bigger; the fibre and berry shrubs with them);
  wildlife 81 sites / 169 creatures -> 156 / 328 (a minute's walk now passes a pack 67-81% of the time).
- **Water** (water). There was no water at all: players walked on the sea floor, and the old MI_Sea plane's collision
  (a 1.2 km square under the island, where fallers landed) is now off. `UWRWaterSubsystem` (`WRWater.cpp`) builds a water
  volume from `Data/sea_mask.json` on the server and every client, at the drawn waterline (80 cm over the mesh,
  `water.surfaceOffset`): swim at 300, tread water with your head out, walk back up the beach. E drinks in the water or
  at its edge (20 Water a gulp; sea water counts, `water.seaDrinkable`). Creatures wade to belly depth and turn back;
  the fall-through rescue now catches swimmers under the island and holds deep fallers until the ground loads.
  Footsteps and the surf start at the drawn waterline (`audio.seaLevel` 60). `Tools/watertest.ps1`.
- **Tools are held** (gather). The hatchet or pick on the hotbar goes in the weapon hand (a model built from engine
  shapes, `WRHeldTool.cpp`) and decides the yield: hatchet on trees, pick on stone and ore, anything else the base rate.
  Every gathering hit costs the held item 1 durability (crude tools 80, stone 160; shown like the sword's); at 0 a tool
  gives no bonus and hits for half until repaired at a workbench (30% of its recipe). As a weapon: 7 / 9 light damage.
- **Bodies to harvest** (gather). A wild creature's kill stays down 180 s (`WRCarcass.cpp`) and nothing goes straight
  into the bag: each hit cuts a share of its loot (a blade keeps all of it, hands or a pick half); the killer and their
  clan have it alone for 30 s. Bosses, tames and wisps die as before. `Tools/gathertest.ps1`.
- **Attack out of a held block** (combat): light or heavy swings at once from the guard, which comes back when the
  swing ends if block is still held (parry still needs a fresh press). Casting while blocking now drops the guard too.
- **Creature wind-ups** (combat): every attack plants, rears back (bite up, stomp on hind legs, charge head down, spit
  head back, lunge crouched), glows red-orange (`/Game/FX/M_WindupGlow`) and makes a deep whoomph (`creature.windup`)
  before it lands; lengths and looks in `telegraph` (bite 0.5 s up to stomp 1.0 s). `Tools/combattest.ps1`.
- **Starter spells** (combat): each path names one (`"starter"`: Fire bolt, Frost shard), granted free at level 1 (and
  when a subclass is taken up), put on wheel 1 so Q casts it; saves without it get it on login, and characters who had
  bought it get the 3 points back.
- **The panel flashing over the map** (ui) was the marker details rebuilding every 0.5 s inside Tick, so new widgets
  drew one frame unmeasured; `WRUI::MeasureNow` / `SetContentMeasured` fix it there and in the other screens that
  rebuild (journal tabs, inventory, paths, skills, storage, crafting, build bar). `Tools/uiflickertest.ps1`. Clicking
  the journal tab that's open no longer closes it.
- **Demolish mode** (ui): a Demolish button in the build bar and a rebindable key (X); the piece under the crosshair
  glows red with its refund ("Demolish Wood wall · 6 wood back"), grey and "Not yours" when it isn't; X, Esc or right
  mouse leave it. `Tools/demolishtest.ps1`.
- **Console** (ui): README.txt now explains the ` key and a few commands (playtest builds: anyone in the game can use
  them).
- **GPU hangs fixed.** The agents' runs hit `DXGI_ERROR_DEVICE_HUNG` now and then, and the merged build hung in 2 of 3
  Epic benchmark runs on a quiet machine, always in the virtual shadow maps' Nanite `PatchSplit` pass: Nanite's runtime
  tessellation, on by default in 5.8. `r.Nanite.Tessellation=0` (DefaultEngine.ini `[SystemSettings]`): 4 of 4 runs
  clean, 8.34 ms (120 fps) on Epic. 0.4.x most likely carried the same risk.
- **A server crash in melee fixed.** The suite's mp run crashed the server in `UWRGameplayAbility::After` called from a
  swing's wind-up step, just after a parry: the step set its re-sweep timers (and the hit could start more), a new timer
  grew the timer manager's storage, and the running closure, `this` included, moved under it. Latent since the combat
  core (2026-10-22); more creatures made it likelier. `After` now keeps each step on the heap and runs it from a local
  reference.
- Tests: the suite gains `combat`, `water`, `gather`, `demolish`, `flicker` and `cave`; several bot tests take `-Port`.
  On the merged build 26 of 27 passed (25 automation tests, 335 checks over 26 bot tests); `mp` was the server crash above, and on the
  fix it passed 3 runs of 9/9, with the ten combat-related suites rerun and passing.
- `ProjectVersion=0.5.0`; packaged and smoke-tested by real input (the starter spell on wheel 1, crafting by click,
  demolish mode on B then X). **Released 2026-10-28 as
  [v0.5.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.5.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB,
  notes `Docs/release-notes/v0.5.0.md`): installed copies update themselves on their next start.
- Known: the volcanic shrubs are still yellow-green (their leaves only recolour through a coat; worth retrying now the
  hangs are understood); ice still uses the old 6 m sheet.

## 2026-10-27: version 0.4.1, menus take clicks again, cave burrowers stay put

Raven's friend reported they couldn't move around the menus, change settings or craft, though they could gather. A new
test plays the menus with a mouse and keyboard the way players do, and it found that in 0.4.0 no menu took a single
click, for the host or anyone who joined.

- **Menus take clicks again.** The polish pass's loading and death cover (`SWRFlowCover`, drawn over the menus) and the
  banner and hint layer (`SWRFlowMoments`) were full-screen widgets left at the default visibility, so even when empty
  they caught every click meant for a menu: tabs, recipes, the Craft button, every setting. Keys still worked (I, J, Esc,
  B open and close them), which is why gathering and opening menus looked fine. Both now let clicks through
  (`SelfHitTestInvisible`); their parts already did.
- **A menu left open under the chat box or the main menu keeps its cursor.** Closing either set game-only input, so a
  menu still open had no cursor and the camera swung with the mouse until it was closed and opened again
  (`FWRPlayerUI::ReapplyInputMode`).
- **Cave burrowers stay put.** A Glacimaw or Kilnmite waiting underground has no collision, so it fell out of the world
  (cave colonies have been empty since they were added); since 0.4.0 the fall-through rescue caught it every second and
  logged it, about 280 times a session. A buried burrower now holds still (`DisableMovement`) and comes out walking, and
  the rescue leaves characters without collision alone.
- `Tools\menutest.ps1` (in `alltests.ps1` as `menus`): a host and a friend who joins from the main menu each open the
  journal with I, click through its tabs, craft a crude hatchet by clicking the recipe and its button, open Settings with
  Esc, flip Vertical sync off and back, click Resume, and open and close the build bar (12 checks). The clicks go through
  Slate as real ones do (`WRClick`, `WRKey`, `WRType`, `WRUIText`, `WRUIState` in `WRUIDrive.cpp`), unlike `WRMenu` and
  `WRSettings`, which call the menus' code directly: that is how a layer eating every click went unnoticed. Bot command
  lists can mark steps `solo:` (on your own, before joining) or `net:` (once in a game), since the list starts over in
  each world.
- Not reproduced: what blocked the friend before 0.4.0, which had neither layer (closing chat over an open menu, fixed
  above, is the one way found). If it happens again, it helps to know whether they host or join, whether a cursor shows
  in the menu, and whether they play with a controller.
- The full suite passes: 21 of 21, 22 automation tests and 246 checks over 20 bot tests. `ProjectVersion=0.4.1`;
  packaged (`C:\Dev\WildreachBuild2`) and smoke-tested by clicking (the packaged game opens Crafting, crafts a crude
  hatchet, opens Settings > Video and resumes). **Released 2026-10-27 as
  [v0.4.1](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.4.1)** (Latest, `Wildreach-Setup.exe` 1.7 GB,
  notes `Docs/release-notes/v0.4.1.md`): installed 0.4.0 and 0.3.0 copies update themselves on their next start.

## 2026-10-27: version 0.4.0, the polish pass

Raven asked for the playtest to feel and look like a real game by polishing what's there, with no new content (plan:
https://claude.ai/artifact/LuauEtwseW8GwRVXie1iM4). Six agents worked side by side in their own git worktrees (animation,
flow and UI, sound, effects and combat feel, world look, performance), on two shared event interfaces that landed
first: `WRSound` and `WRFx` name the moment ("hit.flesh", "gather.wood", "levelup") and data decides what plays.

- **Animation** (`UWRAnimInstance`, C++, no animation blueprint). The player and every creature played one clip at a
  time, so blocking, attacking or casting while moving slid. Now the legs run locomotion (idle / walk / run blended by
  speed and by direction: forward, back, strafes, diagonals) while an upper-body layer from the spine carries block,
  attacks, casts, aiming, reloads and hit reactions; dodge, death, falls, swimming and gathering stay full body. Every
  change crossfades (about 0.2 s), standing turns step the feet round past 70 degrees, feet plant on slopes (two-bone
  IK). Clip paces are measured from the clips' own feet, so steps match the ground speed. Blocking now turns off
  orient-to-movement, as aiming does (the facing used to wobble). 22 new clips (3.7 MB): ranged strafes and back-walks
  built from packs already in the project; the sword-and-shield strafes were already imported.
  - Creatures crossfade, play their loops at their real ground speed, turn at 160 / 260 degrees a second (was 400)
    and arc instead of spinning, bite from the neck up while running, and time each attack's strike to the damage;
    lunge, charge, stomp and spit each move at their own pace. Every creature from `rig_quadruped.py` had its walk and
    run playing backward (moonwalking); the game now plays those loops reversed (the rig script is the real fix).
  - Footsteps fire at each foot plant.
- **Sound** (`WRSound`, `Data/Audio/sounds.json`, `UWRAudioDirector`). The game had none. 118 events from 352 OGG
  files (18 MB, loaded at runtime from `Data/Audio` and decoded with the engine's Vorbis decoder): footsteps by surface,
  swings, hits, blocks, parries, voices for both bodies, creature calls by species and family, gathering, building,
  doors, crafting, spells, ranged weapons, UI, and the moments. Ambient beds crossfade by side of the ridge, time of day,
  coast, caves, lava and altitude; music plays the menu theme, exploring and night tunes now and then, combat while
  something hunts you and the boss track in a boss fight, ducking under big moments. Settings has an Audio page
  (master, music, effects, ambience, interface). All sources are CC0 except two CC-BY 3.0 ones, credited in
  `Docs/CREDITS.md` and the game folder's `CREDITS.md`: "Lava loop" (Blender Foundation) and "Crossbow sounds"
  (HolgiB).
- **Effects and combat feel** (`WRFx`, `Data/fx.json`, a small C++ sprite particle system: Niagara can't be authored
  headless here). Fire and frost bolts with cores, trails and light; Fire wall, Lava pool, Frost nova, Ice lance and the
  rest of the spells; impacts (blood, dust, ice or magic by creature, sparks on blocks); hit-stop (60 ms, 90 on
  parries, animation only), camera shake, a parry flare and screen-edge flash, a crosshair hit marker; damage numbers
  moved to the Slate HUD (gold Cinzel crits, "weak" on boss weaknesses, red for damage you take); wood chips, stone
  dust and plant bits when gathering; dust when building, debris when pieces break; flames on Hearths, torches and
  braziers; embers over lava; snow on the tundra and ash on the volcanic side. Server-only moments reach clients through
  a reliable relay (`AWRFxRelay`).
- **World**: the flat sea is moving water (waves, turquoise shallows, deep water, shore foam; dark at night), meshed
  over the open sea only (`Data/sea_mask.json`). The volcanic side gets a warm ash haze and the tundra a cold blue,
  blended by where the camera is; light shafts at dawn and dusk; mist in 45 valleys; a lifted grade so nights read.
  Build pieces get real shape from code (`WRBuildLook`): plank walls with posts, coursed stone, floorboards and flags,
  shingles and slates on rafters, cut gables, framed doors and windows, palisade points; collision and placement are
  unchanged. The workbench, forge, armorer, alchemy table, Hearth, claim totem and storage chest are modelled props from
  the local AI pipeline (Hunyuan3D 2.0: not licensed in the EU, UK or South Korea). The camera eases back out after a
  wall, widens slightly when sprinting and eases into lock-on.
- **Flow and UI**: dying fades to a carved "You died" card naming what killed you, where your belongings wait and for
  how long, and where you'll wake, then holds "Loading the island…" until the ground around you has streamed in; a
  compass marker leads back to your bag. A loading screen with tips shows when you host, join or travel. Screens fade
  and rise in, buttons and slots warm and dip, pickups stack ("+40 Wood"), notices queue. Banners for level-ups, new
  recipes, first visits to camps, caves, passes and boss lairs, boss title cards and tames. Six one-time hints with the
  real keys ("Show hints" on the Video page). A fixed scenic view behind the main menu.
- **Performance** (`Tools/benchmark.ps1`, results in `Docs/PERFORMANCE.md`): creatures more than 200 m from every
  player think in quarter-second slices (`UWRSignificanceSubsystem`): with all 169 out, server work per frame went from
  14.6 to 6.8 ms. The creature, body and item art preloads in the background at start (`UWRPreloadSubsystem`), which
  removed the two hitches on the walk route (frames over 50 ms: 2-6 per run before, none after). The whole merged pass
  on this PC (RTX 4070 Ti, 1080p): Epic 8.25 ms a frame (121 fps), Medium 5.66 ms (177 fps), p99 9.9 / 7.0 ms, no frame
  over 50 ms; the server 2.0 ms a frame with the usual wildlife and 4.2 ms with all 169 creatures out.
- Fixes found on the way:
  - **Beds never set where you respawn**: the engine reused the controller's first start spot. They work now (and you
    wake standing on the bed, not inside the floor).
  - `M_Rock` lacked the Nanite usage flag, so cooked builds drew the cave boulders in the default material.
  - The sound table had no `hit.dust` or `stomp`, which the effects call.
  - Boss telegraphs and hazard markers were the engine's lit shape material tinted orange, which strong sun blew out
    to flat white or yellow patches; they're now a glowing, see-through, pulsing material (`M_Telegraph`,
    `Tools/fx/make_telegraph_material.py`).
  - New players spawn at 08:00, the peak of the dawn valley mist, so their first look at the island was washed out;
    the dawn mist is about half as strong and clears sooner.
- The full suite passes on the merged build: 22 automation tests and 234 checks over 19 bot tests (`flow` is new);
  `uishots.ps1` captures all 39 screens with no client errors.
- The open items from the first 0.4.0 build, fixed before release:
  - **Falling through the island.** A character that arrived before the ground's collision was there (after a
    teleport, a respawn far away, or a pack spawning) dropped through the terrain onto the map's sea plane 140 m below,
    where the island can't be seen from underneath: the "flat blue ground" in screenshots since 0.3.0. The server now
    checks once a second and puts anyone under the ground back on top (`AWRGameMode::RescueUnderground`; a probe,
    `WRProbe`, logs what's under you). In one test run it caught the player and two creatures.
  - **Distant terrain is drawn.** The ground streamed in only 256 m around you, so from any hill the distance was a
    flat pale void with scalloped edges. Each player now streams four times as far (`world.streamingRangeScale`), and
    trees, rocks and cliffs draw 2.5 times further (`world.decorationDistanceScale`). The benchmark barely moved (Epic
    8.21 ms, Medium 5.74 ms; about 150 MB more memory; streaming after a long jump done in 0.6 s instead of 0.2 s).
  - **Your first spawn follows your path.** The engine chose the start spot before your path was set, so a frost
    player could start (and keep respawning) at a volcanic camp; it's chosen again once your path and save are loaded.
  - **No more washed-out boss arena.** The daytime exposure floor was written in the wrong units (an "EV" of -10 in a
    setting this project reads as luminance), so by day the camera had no floor and brightened dark ground until
    everything on it blew out. It's converted properly now: by day the camera brightens only to an ordinary scene's
    level, under cover (caves, roofs, canopy) it may go further, and nights are unchanged. Daytime valley mist is
    lighter too, so it doesn't read as white blobs from high ground.
- Still known: the volcanic shrubs are bright green; creature feet still slide about half their speed (the procedural
  rigs never fully plant a foot); running strafes show only while aiming or in first person, because third person faces
  where you run.
- `ProjectVersion=0.4.0`. Packaged (`C:\Dev\WildreachBuild2`) and smoke-tested (118 sound events loaded, exposure floor
  -2.0 by day). **Released 2026-10-27 as
  [v0.4.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.4.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB):
  installed 0.3.0 copies update themselves on their next start.

## 2026-10-26: version 0.3.0, the stone-and-leather UI

The whole UI is rebuilt from the mockups Raven approved (dark stone and leather, Cinzel and Alegreya Sans), in Slate
C++ owned per local player by `FWRPlayerUI`. Screens register themselves by name and follow the controller's menu
state, so gameplay still goes through the same server commands (and the bots still drive every menu). Fonts, textures,
item icons, creature portraits and line glyphs load at runtime from `Data/UI` (no editor import). `-oldui` keeps
the old text HUD.

- **Every key is rebindable** (Esc → Controls; saved in `GameUserSettings.ini` `[Wildreach.Keys]`, only what differs
  from the defaults). Click a key and press the new one (keyboard, mouse buttons, wheel); a key already in use asks
  "Replace it?" and the action that loses it shows "Not set". Right-click clears, Reset to defaults. Defaults no longer
  clash: dodge C, crafting J. H shows the current keys in game.
- **Item hotbar (1–8)**: each key holds an item from your bag (drag items onto it in the inventory, right-click or drag
  off to clear; new tools, weapons, food and potions take a free key unless you took them off). Pressing a key equips or
  uses it; the mouse wheel steps along it (it still cycles ammo while aiming). Q/Z/X no longer take a potion, eat or
  drink: the hotbar does that.
- **Ability wheels**: hold Q (or Z for the second wheel) to open a wheel of 8 abilities, pick with the mouse, release to
  keep it; a tap casts the picked one. Or give each ability its own key (cast mode Wheel / Own keys / Both, and
  cast-on-release, in Controls). Fill the wheels from the Paths tab (drag a learned ability onto a slot).
- **Tame orders**: hold E on your tame for an order wheel (follow, stay, ride, release, passive, aggressive); a tap
  opens it to click an order.
- **HUD**: level and path XP, compass with points of interest and the nearest camp or boss, boss bar, clock, vitals with
  food/water/Heat rings, hotbar, ability plate with cooldowns, tame card, taming card, pickups, chat, interaction prompt.
- **The journal** (I / J / P / K / M):
  - Inventory: equipment with a body figure, the 8×5 bag with drag and drop (move, equip, unequip, swap), filters, sort,
    item details (stats, durability, sockets, where it's repaired/made/used), split and drop with a "how many?" picker,
    carry weight, the hotbar strip.
  - Crafting: the station you're at (or by hand) and the others' distance, categories, search, "ready to craft",
    ingredients have/need, quality odds, infusion (melee gear and armor), quantity, repair and upgrade.
  - Paths: the fire/frost tree with learned / available / locked states, node details with real numbers, Learn, and the
    two-wheel loadout.
  - Skills: life-skill cards with level and XP, perk tiers and choices.
  - Map: the island with camps, caves, bosses, passes, your Hearth, claims and tames; zoom, pan, legend toggles, marker
    details (boss loot).
- **Build bar** (B): categories and piece cards with costs, a placing chip with the reason a piece won't go, keys, claim
  status, demolish. **Snap (F) now works**: with it off, pieces go at the aim point (45° turns), checked box against box
  so they can't overlap; client and server check the same spot.
- **Storage** (a chest beside your bag: drag, shift-click a stack, ctrl-click one, "how many?", Store all resources,
  Take all), **Hearth** (tech levels, boss cores, what each level unlocks, stations in the claim, feed a core),
  **Claim** (top-down map of the claim, upkeep per material with time left, stock, radius upgrade), **Taming** (a side
  panel: torpor, taming, effectiveness, foods from your bag to click).
- **Game menu** (Esc): Resume, Controls, Video (window mode, resolution, 3D resolution, frame cap, vsync, quality
  levels, auto-detect), main menu, quit. **Main menu** restyled, with a Settings button and tips that use your keys.
- Gameplay that came with it: the bag has 40 slots; carrying over 80 kg slows you to 70% (`inventory` in
  `tuning.json`); new server commands `move`, `split`, `drop` (a loot bag anyone can open, 10 min), `store`/`take` with
  counts, `storeall`, `takeall`, `hearthfeed`; store/take use the nearest chest (inside a house they found the floor).
- Fixes found on the way:
  - **Claim upkeep cost ten times its rate**: the totem paid each hourly rate rounded up *every minute* (6 wood/h burned
    60). It now pays as it falls due (`FWRBuildRules::PayUpkeepMinute`, tested in `Wildreach.Building.Rules`).
  - **A client crash coming back to a base**: the build pieces' cube mesh and wood/stone materials were held in static
    raw pointers, so after every piece left your client's range they were garbage-collected and the next piece crashed.
  - With Crafting open, Enter also crafted the station's first recipe (the hidden text menu); infusing a bow used up the
    essence for nothing (ranged hits are physical), so the server refuses it.
- Tools: `Tools\uishots.ps1` shoots every new screen (`-MainMenu`: the front end); it and `alltests.ps1` set
  `UE_SKIP_UBT_SDK_SETUP=1`, because every Unreal start otherwise waits on UnrealBuildTool's platform check while any
  build runs. `node Tools/ui_glyphs.mjs` redraws the line glyphs. `menushots.ps1` keeps the old menus (`-oldui`).
- What the mockups showed that the game can't do yet (built the game's way instead): Hearth repair, taking stock back
  from a claim, named chests, map waypoints, audio and mouse settings, the taming stash's contents. Your Hearth and
  claims show on the map only once seen (their locations don't replicate from further than ~150 m).
- Built by five agents in parallel git worktrees (one per screen group) on the shared foundation, then merged. The
  full suite passes on the merged build: 15 automation tests and 222 checks over 18 bot tests; `uishots.ps1` captures
  all 25 in-game screens and the front end with no client errors.
- `ProjectVersion=0.3.0`. Packaged (`C:\Dev\WildreachBuild2`) and smoke-tested: the packaged game loads the UI from its
  `Data/UI` folder (fonts, icons, glyphs) and the menu shows 0.3.0. **Released 2026-10-26 as
  [v0.3.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.3.0)** (Latest, `Wildreach-Setup.exe` 1.6 GB):
  installed 0.2.0 copies update themselves on their next start.

## 2026-10-25: version 0.2.0, the game updates itself

- **Auto-update.** The Wildreach shortcuts now start `WildreachUpdater.exe` (beside `Wildreach.exe`): it asks GitHub
  for the latest release (github.com/.../releases/latest redirects to the tag, so no API limit), and when that's newer
  than `version.txt` it shows a progress window, downloads the release's `Wildreach-Setup.exe`, installs it over the
  folder (saves kept; the running updater renames itself aside to be replaced) and starts the game. Up to date, or no
  network within 5 seconds: it starts the game straight away. "Play without updating" skips it once.
- **The game has a version** (`ProjectVersion=0.2.0` in `Config/DefaultGame.ini`). The main menu shows it, checks
  GitHub too, and offers **Update now** (closes the game, updates, starts it again) when a newer one is out. It's part
  of the network version, so a client only joins a host on the same build; joining a host on a different one says so
  in the menu ("everyone needs the same one") instead of failing mysteriously. Other lost connections are explained
  there too.
- **To release:** bump `ProjectVersion`, `Tools\package.ps1`, `Tools\make_installer.ps1` (writes `version.txt`, builds
  the updater into the game folder), then publish a GitHub release tagged `v<version>` with `Wildreach-Setup.exe`
  attached. Every installed copy picks it up on its next start. Copies installed from the first playtest build
  (before the updater) need this one installed by hand once.
- `Tools\updatertest.ps1`: the update path end to end, offline (two small fake releases built with the real
  installer script, a local stand-in for GitHub): finds and installs the newer one, replaces itself while running,
  starts the game with the shortcut's arguments, then up to date / a non-version tag / no network all start the game.
- `Tools\versiontest.ps1`: a 0.1.9 server turns a 0.2.0 client away, the client's menu says why and (pointed at a
  local feed) offers the newer release; a client on the server's version joins. Automation test `World.Version`
  (which tags count as newer). Both scripts are in `Tools\alltests.ps1`.
- The full suite passes with the wildlife and the updater in: 15 automation tests and 222 checks over 18 bot tests
  (`Tools\alltests.ps1`, about 45 minutes).

## 2026-10-25: the island is full of wildlife

- **Every wild species lives on the island now**, 81 packs and 169 creatures (up from 6 packs plus the cave colonies):
  critters and herds in the lowlands around the camps and along the coasts, predators further out toward the ridge,
  Scorchwings and Magmaws by the volcanic ridge, Rimeclaws and Glacimaws in the tundra highlands, Kilnmite and
  Rimeshell colonies with their egg nests out in the open. Nothing aggressive within 280 m of a player camp, nothing
  in a boss arena, by a mini-boss or at a cave mouth.
- The sites come from **`Data/island_spawns.json`**, generated by `node Tools/terrain/spawns.mjs` from the heightmap
  and the layout (biome by side of the ridge, elevation band by distance from it, flat ground, clear of lava, ice and
  the packs already in the map). The server reads it at start, so the numbers can be changed without rebuilding the
  map.
- **Packs come out near players**: a site spawns its pack when someone comes within 250 m and fades it out 90 s after
  everyone has gone (300 m), so 169 creatures cost only what's around the players. Tamed and knocked-out creatures
  stay; a pack you thinned stays thinned until its respawn time. Tuning: `wildlife` in `tuning.json`
  (`activationRadius`, `leaveRadiusMult`, `despawnSeconds`, `enabled`). Creatures and nests are set on the ground.
- `WRWildlife` (console): sites, how many are out, what lives within 300 m, the nearest quiet site.
- Tests: `World.Wildlife` (every wild species has sites, each in its own biome, on the island, clear of the camps'
  safe zones) and `Tools\wildlifetest.ps1` (a herd comes out on arrival on each side of the island, goes quiet when
  the player leaves, comes back on return; `-Shots` for pictures). Both are in `Tools\alltests.ps1`.

## 2026-10-25: one command runs every test

- **`Tools\alltests.ps1`** builds, then runs the automation tests and every bot test in turn and prints a PASS / FAIL
  table (`-SkipBuild`, `-Only ranged,tame`; each test's full output is kept in the run folder). Every bot test now ends
  with its checks (`PASS` / `FAIL` lines, "N checks, M failed") and exits 1 on a failure.
- New automation tests: `Ranged.Items` (ammo kinds, picking ammo and the wheel, stacking gear, quality, recipes),
  `Taming.Food` (food values, kibble, tame times, every tameable creature eats something), `Creatures.Behaviour`
  (diets, who hunts whom, every hunter has prey, attacks and activity are known), `World.Clock` (night and daylight by
  hour). The rules they check were pulled out into plain functions (`FWRInventoryRules::FindAmmo/NextAmmo`,
  `FWRCreatureDef::FoodValue/CanHunt`, `AWRGameState::IsNightAt/DaylightAt`).
- **`-wrworld=<name>`** gives a server its own world save (`Saved/ServerDB/world_<name>.json`); the arena tests use it,
  so tests running side by side no longer load each other's tames and pieces.
- Fixed: E next to a wild creature and a station opened the station's menu instead of feeding the creature; the nearer
  one wins now.
- Fixed: a creature's lunge, charge or stomp hit any hostile it touched, so a tame's lunge could hit a creature you'd
  knocked out (costing effectiveness); special attacks pass over knocked-out creatures unless that was the target.
- `WRTameAll <order>`: every tame of yours within 30 m takes the order (a whistle: all stay, all follow...).
- The full suite passes: 13 automation tests and 197 checks over 15 bot tests (`Tools\alltests.ps1`, about 40
  minutes). The taming test ran three times in a row clean after its scenario was made deterministic (tames parked,
  the Rimeclaw rooted before it wakes).

## 2026-10-25: test pass

- The older bot tests now check what they test and end like the rest (a PASS/FAIL line per check, "N checks, M
  failed", exit 1 on a failure, no errors in the logs): `bosstest` (16 checks), `buildtest` (16), `mptest` (9),
  `persisttest` (7), `socialtest` (14), `spelltest` (19), `terrain/walktest` (5) and `terrain/hazardtest` (6). Each has
  its own fresh world save (`-wrworld=<test>`) and stops once its bots are done. On the way: buildtest's crop plot was
  placed out of reach and never built, and spelltest learned a branch node before its trunk.

## 2026-10-25: creatures that hunt, flee, sleep and fight differently

- **Attack variety** (`attacks` in `creatures.json`, numbers in `tuning.json` `creatureAI`), each telegraphed: a
  **lunge** that leaps where its target will be (Cinderfang, Gloomhowl, Rimeclaw, Magmaw), a **charge** down a marked
  strip that knocks you back (Emberhorn, Frostantler), a **stomp** that hits everything in a marked ring (Basaltback,
  Woolly drudge), and **spit**, a glob of fire or frost from range (Magmaw spits first then leaps, Scorchwing,
  Glacimaw). Bites are unchanged; tames use their specials too. A rooted or frozen creature can't leap or charge.
- **Food chain**: carnivores get hungry every few minutes and hunt the nearest wild non-carnivore they can take (up
  to 1.3x their size, pack hunters 2x), run it down (pursuit speed, no flanking on running prey, a longer leash on a
  hunt), and the pack shares the kill and eats. Prey that's hunted bolts in bursts (fast ones like the Ashstrider can
  get away); neutral prey fights back. Basaltbacks and Woolly drudges are too big to hunt. Wild creatures otherwise
  still never fight each other.
- **Morale**: a creature below 25% health runs from what's hurting it, or below 50% when a packmate just died;
  passive animals bolt when hit. Bosses and their minions never run.
- **Day and night** (`activity`): day animals sleep at night and night hunters (Gloomhowl, Glowmoth, Aurora fox) by
  day. Asleep, a creature lies still and notices you at a third of the distance ("(asleep)" on its label), and a hit
  wakes it; night hunters notice you from 40% further at night.
- Dev and test commands: `WRSpawnAt <creature> X Y`, `WRCreatureHP <creature> <fraction>`; `Tools/aitest.ps1
  [-Shots]` stages a Cinderfang pack hunt (Pumicehog killed and shared, Ashstrider ignored once they're fed), the
  Gloomhowls asleep by day and hunting at night, a hurt Rimeclaw running, and a charge, stomp, spit and lunges. The
  ranged test now roots its target as it spawns (fresh Cinderfangs lunge at you).

## 2026-10-25: day and night

The world clock (`AWRGameState`, `clock` in tuning.json) now shows on every client, on the island and the arena.

- **Sun and moon**: `AWRSky` (spawned on every machine that renders) turns the map's sun with the hour: it rises in the
  east at dawn (6:00), crosses the south at 65 degrees and sets in the west at dusk (20:00), and the atmosphere reddens
  it on the way down. A moon rides opposite it as a second atmosphere light: a disc in the sky, moonlit clouds and fog.
- **Darker but playable nights**: the sky light is lifted at night so shapes still read, the fog goes deep blue, and
  auto-exposure may only brighten so far (otherwise moonlight looks like day). Numbers in tuning.json `sky`.
- **Lights at night**: the Hearth and the forge have a fire's warm glow (torches and braziers already had theirs).
- **HUD clock**: "Day 3   21:40" top right, pale blue at night.
- **The time carries over a restart**: the world save keeps the clock (`clockHours`); a new world starts at
  `clock.startHour`.
- Building: an **upper wall** (2 m, wood or stone) closes the gap a one-cell sloped roof leaves above its high side.
- `Tools\skytest.ps1` (10 checks): a bot builds a hut with a torch, a brazier, a claim and a Hearth, jumps the clock to
  noon, sunset and midnight and reads the client's sky (`WRSky`) at each, checks the clock runs at clock.dayMinutes and
  that a restart keeps the time. `-Shots` takes the hut and the moon at each time of day; `-Arena` runs it on the arena.

## 2026-10-25: ARK-style taming

- **Knock it out, then feed it.** E on a knocked-out creature opens the **Taming menu**: give it all of a food at once
  (kibble or what it eats) and it eats from that stash on its own, one item every 30 s; taming moves while a meal is
  digesting and stalls when the stash runs dry. The taming times (60 / 300 / 900 s by difficulty) are seconds of eating
  plain food. A creature that **wakes up loses its progress** (the food it hasn't eaten stays in its stash).
- **Kibble** (workbench: cooked meat, 3 berries, an egg, makes 2): every tameable creature eats it, it counts double and
  costs no effectiveness. Eggs come from tamed Snowpuffs, so kibble is a second-tame thing, like ARK.
- **Narcotics keep it under**: each one adds 30% torpor, without the hit that a dart or arrow is. Knocked-out creatures
  drain torpor slower now (250 s from full), so a medium tame fits in one knockout on kibble and a hard one needs
  narcotics.
- **Taming effectiveness** starts at 100%, drops 6% per hit while it's out (tranq ammo too: give narcotics instead) and
  1% per plain-food meal. The tame gets **+health and +bite of up to 50%** from it (`taming.bonusAtFull`); eggs and
  hand-fed calm creatures come out at the full bonus. The bonus is saved with the tame.
- A **taming panel** on the HUD for the nearest knocked-out creature: torpor and when it wakes (from the server's own
  drain, red under 30 s), progress, effectiveness, stash, and "tamed in ~N s", "needs N more raw meat", "it wakes first:
  give it narcotics" or "hungry". `WRTameStat` shows effectiveness, stash, bonus and max health.
- Fixed: your own tames attacked a creature you'd knocked out (they took it as your combat target) and could kill it; tames
  leave knocked-out creatures alone now, and a tame told to stay only fights within 15 m of its post (it crossed the map).
- Fixed: a quick shot could fly the wrong way: the server checked the aim point against its copy of your view, which can
  be a frame behind a flick. The shot now carries the view direction, and the crosshair trace sees creatures (low ones
  sent the shot at the ground behind them).
- Every taming number is in `tuning.json` (`taming`), and the Taming menu / HUD lines are `WRTameGive <food>` and
  `WRNarcotic` for bots. `Tools/tametest.ps1` knocks a Cinderfang out with two tranq arrows, costs it effectiveness with
  a stone arrow, keeps it under with a narcotic and tames it on kibble (+47%), lets a part-tamed Rimeclaw wake up (back to
  0%), and checks the tames and their bonus survive a restart; `-Shots` takes the panel and menu screenshots.

## 2026-10-25: building a house

A two-room, two-storey house with stairs and a gable roof now goes up the way a player expects, and
`Tools\housetest.ps1` builds one. Before, only a one-room box worked: pieces could be stacked into the same spot, the
ghost showed green for things the server then refused, outer corners had holes, the "roof" was a second flat floor,
the stairs a single plank, a wooden building past two storeys collapsed, and nothing could be removed in game.

- **Ceilings sit on walls, a second floor you walk on**: the build ghost picks the storey you're looking into (a floor's
  surface counts as its storey, so aiming at the upper part of a wall no longer jumps to the next one); floors / ceilings,
  roofs and gables go on top of it. Walls stacked straight on walls, pillars or foundations cost no span (they carry
  the weight down), so a wooden building can go up storey after storey; reaching sideways still uses wood's 4 / stone's 6.
- **One piece per spot**: a door, doorway, window, wall or gable can't go on an edge that already has one, a floor or
  roof not into a cell that has one, and stairs keep the opening above them clear. The ghost turns red with the reason
  (no support, something is already there, the stairs need this opening) before you click: support values replicate,
  so the client checks the same rules as the server.
- **New pieces**: a **doorway** without a door (between rooms), a **sloped roof** (rises 2 m across a cell, with eaves;
  R turns which way, two facing ones meet at a ridge) and a **gable** (the triangle that closes a roof's side; R flips
  it). **Stairs** are eight treads between stringers with a hidden ramp you walk up, ending level with the floor above.
  Walls, windows and doorways run 10 cm past each end so corners close.
- **Planks and stone blocks**: built pieces wear world-aligned materials (`Tools/make_build_materials.py`,
  `/Game/Building`): 20 cm boards with staggered joints and grain for wood, 45 x 90 cm blocks in running bond for stone.
  The ghost keeps its green / red tint.
- **Demolish mode**: the top of the build menu; aim at a piece of yours (it turns red), LMB removes it for half its
  cost. `WRDemolish x y z` does the same from the console.
- **Build menu** in groups with headings (wood building, stone building, crafting/storage/beds, land claim, defense,
  decor); names say what pieces are for ("Wood floor / ceiling", "Wood stairs (one storey)").
- `Tools\housetest.ps1` (25 checks): foundations, a wall, a ceiling and an upstairs wall go through the ghost exactly as a
  player aims them (and land where expected); the expected refusals happen; the bot demolishes a pillar, climbs the
  stairs to the first floor, and walks in through the front door and the inner doorway; a server restart loads all 34
  pieces back. `-Shots` adds screenshots. The survival test's hut now gets a sloped roof and two gables.
  Bots: `WRGhost <piece>` + `WRLook` + `WRGhostPlace` place through the ghost. `Wildreach.Building.Rules` covers the
  snapping, slots, support and storey rules.

## 2026-10-25: ranged weapons are crafted

- **By hand** (B menu): throwing spear (4 wood, 2 stone, 2 fiber), bola (6 fiber, 3 stone), 3 stone arrows
  (wood, stone, fiber), tranq arrow (stone arrow + narcotic), tranq bolt (crossbow bolt + narcotic).
- **Workbench**: hunting bow (10 wood, 12 fiber, 2 hide), blowgun (6 wood, 4 fiber, 1 hide).
  **Forge**: 6 crossbow bolts (copper ingot, 2 wood); the crossbow at Hearth tier 2 (3 bronze ingots, 8 wood, 6 fiber).
  **Alchemy table**: the new **narcotic** (3 berries steeped with a raw meat, makes 2), and torpor darts now take
  narcotic + fiber (was 2 berries, 2 fiber), so every knockout goes through it.
- Bows, crossbows and blowguns roll **quality** like swords: quality x roll scales the weapon's own damage and torpor
  (the ammo's share is fixed), each shot wears it by 1 (`ranged.wearPerShot`), and worn out it hits at half.
  It's repaired where it's made: the station menu shows "Repair Hunting bow (x/250)" at the workbench, and the forge
  says where to go. Spears and bolas stack, so they're crafted in stacks like ammo, not one rolled item each.
- `tuning.json` has a `ranged` section with every ranged number the code was defaulting (aiming camera, spread, throw
  release, projectile size and lifetime, bola limits, wear).
- The rules test still checks the crude tools need only wood and stone; every other hand recipe now has to take
  things gathered bare-handed (wood, stone, fiber, berries) or made by another recipe, and every ranged weapon and
  ammo has to have a recipe. `Tools/crafttest.ps1` crafts all of them at the arena's stations, then wears a bow by
  shooting and repairs it at the workbench (the forge refuses).

## 2026-10-25: ranged weapons

- **A ranged slot** next to the sword and shield: a hunting bow (stone and tranq arrows), a crossbow (bolts and tranq
  bolts), a blowgun (torpor darts), throwing spears (stack of 5) and bolas (stack of 10). **G** swaps between the
  sword and shield and the ranged weapon, **right mouse** aims (the camera pulls in over the shoulder, the view
  narrows, you walk at half speed and face where you look), **left mouse** fires or throws at the crosshair, and the
  **mouse wheel** picks the ammo. The HUD shows the weapon and what it's loaded with; firing from the hip spreads 4°.
  A bite that staggers you also stops you shooting.
- Shots are real projectiles with drop. Arrows, bolts, darts and spears **stick** where they land, tip first (and ride
  along on a creature they hit); a thrown spear is left on the ground to pick up again. A **bola** roots small
  creatures (new `Rooted` status, no movement) and slows big ones. Tranq ammo adds **torpor** on the hit, the start of
  taming: two tranq arrows or two torpor darts knock a Cinderfang out.
- Art: AI-made bow, crossbow, blowgun, spear, bola and arrow (`batch_ranged.json`; `normalize_gear.py` bow / pole /
  ball modes). Pole mode now turns an item onto its principal axes first: image-to-3D left the blowgun, spear and
  arrow lying on a slant, so they were held and flew crooked. 13 new Mixamo clips on both bodies: bow idle / walk /
  run / draw / aim / aim-walk / release / equip / put away, rifle aim / fire / reload (crossbow and blowgun), and a
  throw. Each shot plays its fire clip and then its reload.
- Holding them: `ranged.socket / along / face` in `items.json`, worked out from the skeleton. `"pose"` measures those
  directions in a body clip instead of the reference pose (arms hanging), and `"along": "hands"` runs the weapon level
  from the trigger hand toward the other hand: the crossbow and blowgun sit at the cheek in the rifle-aim pose, and the
  spear is carried level, blade forward. Mixamo's rifle aim holds the gun ~25° left of where the body faces, so the
  body turns that much while aiming one.
- Adding clips to a survivor without touching its mesh: `Tools/import_anims.py` imports animations only onto the
  existing skeleton (Interchange, through a pipeline stack override: a bare pipeline in the import task is ignored
  and the default one imports a new mesh and skeleton). `survivor_build.py --rest-from` builds the clip FBX on the
  survivor's own rest pose and strips tracks for bones the survivor lacks (the FBX exporter drops any clip that has one).
- The crosshair and `WRAimAt` use the survivor's own camera, so the art camera can watch a bot shoot.
- Bot and test commands: `WRRanged 0|1`, `WRAim 0|1`, `WRFire`, `WRAmmo 1|-1`, `WRAimAt <creature>` (holds over for
  drop), `WRHold <seconds>` (roots the wild creatures within 30 m), `WRClearCreatures` (removes them and the training
  dummies). `Tools/rangedtest.ps1 [-Body f]` fires every weapon at a fresh, rooted Cinderfang on the arena with
  screenshots: 9/10 hits on the male body (the spear fell short), 10/10 on the female, no errors.

## 2026-10-25: the survival loop works from a fresh spawn

A playthrough as a fresh bot on `L_Island` showed the basic loop didn't hold together: the island's trees, rocks and
shrubs were decoration (swinging at them gave nothing; the only gatherables were ~190 placeholder cones over 16 km²),
there were no tools, nothing could be made without a station, and the workbench only cooked meat and made waterskins.
Now a fresh survivor can do it all, and `Tools\survivaltest.ps1` plays it end to end.

- **Gather from the island itself**: a melee swing chops the tree, breaks the rock or picks the shrub in front of you
  (trees -> wood, rocks -> stone, shrubs -> fiber and sometimes berries). Each takes a number of hits, then vanishes
  for everyone and grows back later (`gathering` in `tuning.json`: per hit, hits, minutes). The used-up ones replicate
  through a small `AWRHarvestLedger`; the decoration itself stays local. Every gather shows a notice ("+12 Wood
  (Crude hatchet)").
- **Tools**: a crude hatchet and a crude pick, made **by hand** from wood and stone (they're at the top of the build
  menu, B), and a stone hatchet and stone pickaxe at the **workbench** (wood, stone, fiber). Tools work from your bag:
  the best hatchet you carry chops, the best pick breaks stone, ore and sulfur. Bare hands 1x, crude 1.5x, stone 2x
  (`"tool": {"kind", "yield"}` in `items.json`). Recipes with station `hand` are made anywhere.
- **Foundations sit into the ground** (`building.foundationSink`, 60 cm) so the floor is a walkable step instead of a
  waist-high ledge, and the slab runs deep, so slopes don't show a gap under it. A new foundation next to one you built
  levels with it; otherwise it sits on the ground at its cell's centre, wherever you aim.
- **Placing a workbench, forge, Hearth or totem shows a preview**: the ghost was invisible (those are child actors only
  the server spawns).
- **Doors swing on their hinge** instead of turning in the middle of the doorway.
- Sulfur now counts as mining (pick, mining XP), as the chop clip already assumed.
- **E uses the nearer** of a door / chest / bed / crop / claim and a station, which is what the prompt already showed:
  a workbench inside a small hut no longer opens its menu when you go to close the door, and the server's interact
  picks the door rather than the wall beside it. The build menu is titled "Build & craft".
- The **chop clip** plays at island trees and rocks too, and the gather crouch at shrubs.
- `WRGather tree|rock|shrub|node|any|off`: the test bot walks to the nearest one and gathers with light attacks.
- `Tools\survivaltest.ps1` (23 checks): a fresh fire survivor on the island gathers bare-handed, crafts the crude
  tools, gathers more with them, builds a hut (foundation, three walls, a door, a roof) and a workbench out of what it
  gathered, opens the door and walks in, crafts the stone tools at the bench, closes the door from inside and gathers
  more again. `-Shots` adds screenshots (a tree before and after felling, the hut from three sides and through the
  door, a chop). It backs up the island's world save and puts it back.
- Known: tools don't wear out yet (durability is only saved for equipment).

## 2026-10-25: the humanoid bosses have bodies

- **Vulkarn, Grimjaw and the Obsidian Warden** are AI-made, rigged on Mixamo and animated with its Creature Pack
  (breathing idle, walking, run, swipe / punch / jump-attack attacks, roaring, dying). Creatures with several attack
  clips (`attack`, `attack2`, `attack3` in `creatures.json`) take them in turn, and every creature attack clip is now
  fitted to the swing's length, so the hit lands with the telegraph. Bosses **roar** when they enter a new phase
  (`roar` clip; `Phase` replicates with a notify).
- **Ignessa** is a winged fire fae: `rig_special.py --body fae` gives her a body, a head and two hinged wings that fan
  while she floats; her fly / hover clips map to Run / Idle.
- Every creature now has art except the training dummy. The **wisp** (Ignessa's summons) is a glowing orb: creatures with a `glow` value and no mesh get an emissive sphere in their colour (`/Game/Creatures/M_Glow`, made by `Tools/make_glow_material.py`) and a point light that bright.
- Image-to-3D brutes often fuse a hand to a thigh: `survivor_build.py --cut-stretch 4` drops faces that stretch past
  4x their length during any clip (only the clips count; the T-pose bind itself is ignored) and the scraps left over,
  then closes each hole it leaves with a patch textured from the faces around it.
- Fixed: the shield could lie flat like a tray. The skeleton's reference pose in Unreal isn't always the T-pose (it
  can be a clip's first frame, arms hanging), so the shield's face now comes from the arm itself: away from it,
  between up and out from the body.
- Mixamo without Raven at the keyboard: the decimated copy goes into the page as text; wide bodies get a tiny
  framing triangle so the marker view shows them whole (`Tools/art/README.md`).
- Fixed: packaging crashed cooking the Barghest pack's mane and tail clips (`Assertion failed: !Hash.IsZero()`: they
  have no skeleton in the pack). The game never used them; they and their meshes moved out of `Content` (kept in
  `C:\Dev\WildreachArt\replaced\barghest_parts`). The pack is git-ignored, so re-adding it from Fab brings them back.

## 2026-10-24: a second body, a real sword and shield

- **Female survivor**: the second AI survivor is rigged on Mixamo with the same 55 animations
  (`/Game/Characters/SurvivorF`). The main menu has a **Male / Female** choice, sent as `?Body=f` on the join URL,
  kept on the PlayerState and replicated with each pawn, so everyone sees the body you picked. Bodies are variants
  under `"bodies"` in `Data/player_art.json`; each overrides any top-level field.
- **Real sword and shield**: an AI-made one-handed sword in the right hand and a round iron-rimmed shield strapped to
  the left forearm, replacing the cube blade. `Tools/art/normalize_gear.py` puts the grip at the origin and trims
  stray struts off the shield's back. The game works out the blade and shield-face directions from the mesh bounds and
  the T-pose skeleton (`weaponMesh`, `shieldMesh`, `shieldSocket`). First person hides the shield; the sword stays.
- **Gathering clips play**: swinging at a resource node right in front does an overhead chop for wood, stone and ore,
  or a crouching gather for plants; looting a bag, taking an egg or feeding a wild creature plays a pick-up for
  everyone.
- **Jump fixed**: the jump clip no longer adds its own hop on top of the real jump, and the fall starts airborne.
- `Tools/art/survivor_build.py --cut-stretch 2.5` removes faces that tear when animated (image-to-3D bodies sometimes
  fuse a hand to a thigh), measured against the model's own pose, plus the scraps it leaves.
- Fixed: `Tools/import_prop.py` re-parented every prop material under `/Game/Props` (the caves' tinted rock
  included) to the plain prop material. It now fixes only what it imports.
- `Tools/playershots.ps1 -Body f` frames the female body.

## 2026-10-24: the player has a body

- **Custom survivor** (Raven's pick over the UE mannequin): an AI-made male survivor (T-pose concept, so Mixamo can rig
  it), rigged on Mixamo and animated with the Sword And Shield pack (49 moves) plus a dodge roll, an overhead chop,
  gathering, picking up, swimming and treading water. 55 clips in `/Game/Characters/Survivor`.
- `Data/player_art.json` drives it: mesh, facing, and which clips play for idle / walk / run (play rate matched to the
  real speed), falling, blocking, dodging, swimming, light and heavy swings (fitted to the swing's length), spell casts,
  hit flinches and death. The placeholder blade is now a sword held in the right hand, its grip worked out from the
  skeleton. First person hides the body.
- `Tools/art/survivor_build.py` copies Mixamo's rig from the decimated copy it was made on onto the full textured mesh,
  moves the rest pose to the animations' T-pose and makes the clips play in place; see `Tools/art/README.md` for the
  Mixamo steps (and the page's quirks). `Tools/playershots.ps1` frames the player with `WRArtCam me`.
- Known: the female survivor and the four humanoid bosses are built and Mixamo-ready but not rigged yet (the bosses'
  arms-down poses will need T-pose versions); gathering, chopping and pick-up clips are loaded but not yet triggered by
  the game; the jump clip's own hop adds to the real jump; the sword is still the placeholder cube.

## 2026-10-24: real art, part 2 (every wild creature, terrain polish)

- **Every wild creature now has art** (19 of 25 creatures; the four humanoid bosses and the wisp remain):
  Glowmoth (flat moth that hovers and flaps), Glacimaw (ice worm that slithers and rears to strike) and Snowpuff
  (fluffy flightless bird) from the AI pipeline, with new rigs (`rig_special.py --body moth|worm`, `--legs 2`);
  Gloomhowl (pale Barghest), Scorchwing (ember Mountain Dragon) and Skaldrith (ice Mountain Dragon, the boss) from the
  QuadrapedCreatures pack with tinted materials (`Tools/art/creature_tints.py`, `wire_pack_creatures.mjs`).
- Creatures can override materials per slot (`materials` in `creatures.json`), and flyers play `fly` / `hover`
  animations while airborne.
- **Lava and ice are one continuous surface** each (procedural mesh with shared corners), so slopes are smooth
  instead of stepped tiles.
- **Caves are sculpted in Blender** (`Tools/art/cave_gen.py`, Raven asked for this over the blocky look): a rough rock
  mound with a domed chamber (~24 m across, ~10 m high) and a ragged, recessed entrance tunnel carved out, three
  variants picked by position, tinted rock per biome. Collision is the mesh itself (`Tools/import_cave.py`) and the
  landscape stays the floor. The old block shell (now boulder-covered) is only a fallback.
- **Ridge cliffs**: 4.7k large boulders on the ridge's steep faces (`basalt_cliff` / `ice_cliff`), hiding the
  stretched landscape triangles there. Decoration only, so the ridge is still a wall. Existing placements unchanged.
- **More trees**: charred trees come in four models (two new: a split snag and an ember-lit dead tree) and the
  tundra mixes five (spruce, wind-bent mountain pine, snow-laden fir and a frosted birch are new).
- **Fixed: trees floated on slopes** (Raven spotted it): every tree, rock and shrub now stands on the lowest ground
  under its footprint (`dressing.mjs`), so the uphill side buries instead of the downhill roots hanging in the air.
  Positions are unchanged; trees dropped 0.9 m on average.
- **Fixed: ice sheets stretched into the sky** at the glacier plateau's edge; surface corners now only join cells
  within 7 m of each other (steep river beds stay joined, plateau edges split).
- Packaging: the pack creatures' meshes and animations are in `DirectoriesToAlwaysCook`.
- Checks: `Tools/terrain/tree_shots.ps1` takes eye-level shots among the trees in both biomes.

## 2026-10-24: real art, part 1 (creatures and terrain)

- **13 creatures made with the local AI pipeline** (`Tools/art/README.md`): Pumicehog, the nine four-legged ones
  (Magmaw, Basaltback, Emberhorn, Ashstrider, Rimeclaw, Frostantler, Rimeshell, Aurora fox, Pale Hunter) and the three
  six-legged ones (Cinderfang, Woolly drudge, Kilnmite). Each has Idle/Walk/Run/Attack/Hit/Death.
  The rigger does six legs (tripod gait), adds a middle leg pair to four-legged models (`--add-mid-legs`), and
  lengthens foreshortened torsos (`--stretch`). `Tools/creatureshots.ps1` + `WRArtCam` screenshot a batch in game.
- **Terrain textures** (`Tools/terrain/build_terrain_materials.py`): the landscape material uses the MW Landscape
  Auto Material pack: dark ash with stony patches and black sand in the west, snow and pale sand in the east, basalt /
  frost rock on steep slopes. New `M_Rock` (boulders, cave walls), `M_Lava` (glowing, flowing) and `M_Ice` (glossy).
- **Dressing uses real meshes**: AI-made charred trees (two kinds) and snowy pines (`Tools/import_prop.py` ->
  `/Game/Props`, Nanite, invisible trunk collision as before), SimpleRocks boulders (same footprint and collision as
  before), GV shrubs, and rock-textured cave walls; falls back to the engine shapes when a pack or prop is missing.
- **Packaging**: creature art and dressing meshes are loaded by path, so they're now listed in
  `DirectoriesToAlwaysCook` (a packaged build would otherwise have shipped placeholder shapes).
- **Fixed**: art creatures whose radius exceeds their half-height floated (the capsule grows to the radius); the
  rigger ignored `--head`; re-importing a creature kept the old texture and animations (the importer now refuses).
- Known: lava tiles step visibly on slopes (needs one continuous surface); shrubs stay green in the volcanic west (the
  pack's leaf colour ignores tints); one pine model for the whole tundra; cave shells are still boxes (now rock-textured);
  the sawtooth shadow on the steep ridge face remains.

## 2026-10-24: playable with friends

- **Main menu** (shown when the game runs on its own): name, Fire or Frost, **Host** (your game becomes a listen
  server on UDP 7777) or **Join** by IP; remembered between runs. F10 returns to it. `Tools/package.ps1` makes a
  shareable Windows build with a players' README.
- **Paths menu (P)**: both trees with ranks, costs, branch gates and points to spend; Enter learns.
- **Every tree node works**: spells Fire wall, Lava pool, Meteor (fire) and Frost nova, Ice lance, Ice wall,
  Blizzard (frost); weapon arts Molten edge, Molten cleave (leaves lava), Flurry; passives spreading flames, fuel the
  fire, wildfire, explosive bolt, overheat, searing parry, brittle, cold return, chain shatter, cold riposte, frozen
  moment, whiteout. Numbers in `tuning.json` `spells.*` / `passives.*`.
- **Map list (M)**: bosses, mini-bosses, passes, camps and caves by distance with a compass direction.
  **Chat (T)**: everyone on the server sees it.
- **Island dressing**: 26k placeholder trees, rocks and shrubs and six midland caves (rock tunnel + dark chamber
  with rich ore at 12% gem odds, sulfur gas or slick ice, a Kilnmite or Glacimaw colony), all generated from the
  heightmap into `Data/island_dressing.json` and built on every machine at runtime.
- **Biome gear checks**: the lava river burns without lava resistance (fire main path, fire-infused armor, or a
  mount); slick ice on the tundra rivers and glacier plateau without ice resistance (frost main path, frost-infused
  armor). HUD warning when you're standing in one.
- **Fixed**: clients used their own default server preset, so lock-on and PvP disagreed with the host; the host's
  settings and everyone's clan now replicate. The tame test no longer races a wandering herd animal.
- Tests: `spelltest.ps1`, `terrain/hazardtest.ps1`; `island_shots.ps1` also covers caves, lava and ice.
- Known: hazard tiles are flat placeholder slabs; decorations and caves stop drawing at ~250 m (the World
  Partition loading range; HLODs would fix distant views); a dedicated-server package waits on the source engine build.

## 2026-10-23/24: the rest of the first playable (batches 1–5)

Everything below is placeholder art (engine shapes, tinted) with numbers in `Data/*.json`, all first drafts to tune in playtests. Each batch has a bot test script in `Tools/`.

- **Crafting & equipment** (`crafttest.ps1`): recipes by station (forge, armorer, alchemy table, workbench) from `Data/recipes.json`; quality tiers Crude→Masterwork with stat rolls; gem sockets; fire/frost infusion with path essences; repair and +1..+10 upgrades with shards; the Hearth's boss cores raise station tier. Weapon/armor durability, armor damage reduction.
- **Building** (`buildtest.ps1`): snap grid, foundations/walls/floors/doors/gates/chests/beds/crops/pens/stations from `Data/pieces.json`; support (wood spans 4, stone 6) with cascade collapse; code locks; land claim totems with upkeep and decay; raids damage pieces; the world saves to `Saved/ServerDB/world_<map>.json`.
- **Creatures & taming** (`tametest.ps1`): the 17-creature roster (pack hunters, flyers, burrowers, herds, critters); knockout + feed, offer food, steal an egg (nests with angry guardians) and path magic; torpor darts; tame orders (follow, stay, aggressive, passive, release); rideable mounts; Glowmoth light, ore-mining tames, pen produce; 2-tame limit; tames saved with the world.
- **Bosses** (`bosstest.ps1`): Vulkarn (anvils vs lava floods, a tongs grab you can parry, white-hot phase 2, hidden phase 3), Skaldrith (ice pillars block breath and shatter, flight and dives, hit his wings to ground him, whiteout phase 3), Obsidian Warden (shell; chill cracks it faster), Ignessa (airborne, exploding wisps, lands when they're gone), Grimjaw (regenerates unless burning), the Pale Hunter (invisible in blizzards, footprints, fire reveals). Telegraphed attacks, HP +80% per extra player or tame, ≥10% of the damage earns your own loot (cores, essences, parts, shards, trophies), boss HUD bar, respawn time from the server preset.
- **Life skills, server settings, clans** (`socialtest.ps1`): 15 life skills (levels 1–100, perks at 25/50/75/100; K opens the menu) wired into gathering, crafting, potions, food, crops, pens, building, taming and loot. Server presets `pvp_standard`, `pve`, `casual_coop`, `playtest` (default: 10× XP) and `custom` (`Saved/ServerDB/server.json`), picked with `-wrpreset=`. Clans (`WRClan create/invite/join/leave/kick/info`) share claims, doors, chests and tames, and can't hurt each other. Safe zones at the spawns. PvP and PvP-in-boss-arenas switches, offline base protection, death penalty gentle / Souls-like / drop / full loot with loot bags.
- **Island rebuilt** with the six boss spawners, spawn safe zones and starter camps; walk test still passes.
- Tuning overrides for tests: `-wrtune=key:value,key:value`.
- Known gaps (caves and foliage arrived 2026-10-24): fishing, engineering and swimming have no XP source; perks marked "not active yet" wait for their systems; the bounty and creature-raid settings exist but nothing uses them; boss loot goes straight to inventory (no loot bags).

## 2026-10-23: the island (M3 step 2)

- **Layout approved** (`Docs/terrain/LAYOUT_PROPOSAL.md`): island, hard-wall ridge, two passes, Vulkarn in the crater, Skaldrith on the glacier bowl, one water + one lava river on the volcanic side, middle spawns moved toward the passes, ~10 km² for the first playtest.
- **`L_Island`**: the full 4 × 4 km landscape (4033 × 4033 samples at 1 m, Z range ±1 km) as a World Partition landscape in 256 streaming pieces. It's now the default map for `play.ps1`, the server and the editor.
  - Built by `Tools/terrain/build_island.ps1`: Node exports the heightmap, then the editor (a window opens and closes by itself) imports it through `UWREditorTerrainLibrary`, waits for the landscape's GPU layer update, and saves.
  - Placeholder auto-material: dark ash west, snow east, rock on steep slopes. Sea plane, sky and lighting.
  - From the layout data: 6 spawns (biome-tagged), fire/frost altars at the two boss sites, 6 creature packs by zone, 124 resource nodes (wood/stone/copper everywhere, sulfur west, lichen east).
- **Walk test** (`Tools/terrain/walktest.ps1`), with real character movement: north-west spawn → north pass in 3.6 min, then onward to the tundra (5.9 min total). Middle spawn → north pass in 5.6 min, fording the river. Walking at the ridge mid-island gets stuck, so the wall holds.
- Fixed: rivers were 22 m deep trenches that trapped players; now 6 m deep, fordable, with gentle banks.
- Dev commands: `WRTeleport x y`, `WRLook yaw pitch`, `WRWalkTo x y`. `play.ps1` takes `-Map island|arena|grid -Path fire|frost -Name You`.
- Known: placeholder material only (no textures); rock barely shows on the ridge; a sawtooth shadow artifact on the steep ridge face at distance; no PCG foliage yet.

## 2026-10-22: M1 combat core + M2 paths (autonomous build session)

### Combat (M1)
- **GAS** set up: ability system on the PlayerState for survivors (survives death), on the pawn for creatures. One replicated attribute set: health, stamina, poise, hunger, thirst, Heat, Chill, burn/frost buildup.
- **One damage pipeline** (`UWRCombatLibrary::ApplyHit`, server only) used by every source: block (front 70°, stamina cost 10–25 by hit size, guard break at 0 stamina), parry (first 0.2 s of a block, staggers the attacker), dodge i-frames, poise → stagger → riposte (×3), Heat damage bonus (up to +30%), frostbite (+15% damage taken), Permafrost (+15% vs slowed).
- **Statuses:** Burn (DOT), Frostbite + Slowed, Frozen (frostbite on an already slowed target), Shatter (heavy hit on frozen; spends all your Chill for up to double the burst), Stagger. Replicated as gameplay tags; bodies tint by status.
- **Abilities:** light attack, heavy attack, dodge roll (root motion), block/parry (hold), Fire bolt, Frost shard, Flame slash, Ice slash, creature bite. Stamina costs and timings from the Starting numbers tab.
- **Combos:** Thaw (25/25: fire on a frozen target bursts and thaws it) and Balance (50/50: Heat and Chill feed each other).
- **Netcode:** attack/block slowdowns and sprint are predicted movement flags (no rubber-banding); hits are confirmed by the server.

### Paths (M2)
- Fire and Frost trees generated from the design doc (`Tools/data/extract_trees.mjs` → `Data/paths/*.json`).
- XP curve (point n costs 100 + 10n), 150 total / 100 per path, trunk-first, branch gates by points spent, root passive on the main path only, quick slots (8, auto-filled), character level = points ÷ 3 driving health/stamina (exactly 250/150 at 50).
- Main path from the join URL until there's a creation screen; spawn in its biome. **Boss altars** (E to interact) take/swap the subclass; progress is kept per path.
- **Persistence v1:** JSON per player in `Saved/ServerDB/players/`, autosave every 10 s, on logout and shutdown.

### Creatures & world
- `Data/creatures.json` (dummy, Cinderfang, Gloomhowl, Rimeclaw, Magmaw); stats from the zone tier's starting numbers.
- Training dummy (never dies, resets), pack-hunter AI (flanking, leash, give-up), creature spawner.
- `L_CombatArena` test map (dummies, both packs, both altars, biome-tagged spawns).

### Tools & tests
- `Tools/test.ps1`: 6 automation suites: XP curve, character level, tree shape, buy rules/caps, damage math, and a 12-step deterministic combat scenario in a real world. All pass.
- `Tools/mptest.ps1`: dedicated server + scripted bot clients (PvE/PvP), summarises hits/statuses/abilities/corrections from the logs.
- `Tools/persisttest.ps1`: build a character, restart the server, rejoin, check it's intact.
- `Tools/shots.ps1`: rendered client takes screenshots from both camera views.
- Dev commands (console / `-wrbotcmds`): `WRGivePoints`, `WRAutoSpend`, `WRBuy`, `WRSetMain`, `WRSetSub`, `WRWeapon`, `WRSpawn`, `WRStatus`, `WRHeal`, `WRKillMe`, `WRStarve`, `WRGoToAltar`, `WRInteract`, `WRView`, `WRBotPause`, `WR.ReloadTuning`.

### Stat nodes
- 13 stat nodes now do something (data-driven via `nodeEffects` in tuning): ignite chance, searing heat, long burn, crushing blast, heated blade, bellows, tempered, frostbite, deep freeze, hard break, keen edge (crits), light step, chilling touch. Ability nodes past the trunk (Fire wall, Meteor, Ice lance...) are still to build.

### Inventory (M5 start)
- 30-slot inventory on the PlayerState (server-authoritative, owner-replicated), items from `Data/items.json`, stacking + weight, starting kit (3 minor healing potions, 2 cooked meat, 2 waterskins).
- **Q** drinks a potion (healing only from potions, 6 s belt cooldown), **Z** eats, **X** drinks. HUD belt line shows counts, cooldown and carry weight. Inventory saves with the player.
- Dev command `WRGive <item> <count>`.
- **Gathering:** resource nodes (`AWRResourceNode`: wood, stone, copper ore, sulfur crystal, lichen). Melee swings harvest them (heavy = double), nodes deplete and respawn. Arena has 8 nodes. Test bot mode `-wrbotgather`.

### Terrain (M3 step 1)
- Landmass proposal for sign-off: `Docs/terrain/LAYOUT_PROPOSAL.md` (island, meandering ridge, two verified passes, volcano + glacier boss sites, rivers, spawns, walk times). Nothing imported yet.

### Known gaps
- Placeholder art only (no downloads this session): greybox bodies, no animations.
- Stat/ability nodes beyond the four trunk abilities and the capacity nodes are tracked and buyable but have no gameplay effect yet.
- No lag compensation for melee yet; ~0.3 small movement corrections/s per client in bot tests, likely pawn-on-pawn collisions.
- Save key is the player name (`?WRName=`); real account ids come with EOS/Steam (M7).
