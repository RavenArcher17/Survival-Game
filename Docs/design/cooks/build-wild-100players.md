# Wildreach cook: 100 PLAYERS ON A SERVER (lane build-wild-100players, 2026-11-16)

Aaron 11-16 (after talking to Raven; q-server-32 was "Keep 32 ... Maybe 100 later, Raven will say"):
**"Up to 100 players at a time in a server."**

## 0. Requirement ledger

| # | Requirement (from the brief) | Plan | Proof (filled at the end) |
|---|---|---|---|
| R1 | Dedicated servers support up to 100 concurrent players | 3.1-3.9 | |
| R2 | The cap: default 100 on dedicated servers | 3.1 | |
| R3 | The cap host-chosen lower (dedicated and listen) | 3.1 | |
| R4 | Listen-server hosts kept at a sane lower cap | 3.1 | |
| R5 | Replication scaled: Iris or a Replication Graph | 3.2 | |
| R6 | Relevancy / dormancy for builds | 3.3 | |
| R7 | Relevancy / budgets for creatures (AI/creature replication budgets) | 3.4 | |
| R8 | Relevancy / dormancy for items (loot bags, nodes, eggs, rubble) | 3.3 | |
| R9 | Relevancy for projectiles | 3.4 | |
| R10 | Server performance budgets (tick, bandwidth per client, update rates, cull distances, spatial partitioning) | 3.4, 3.5 | |
| R11 | The players list at 100 | 3.7 | |
| R12 | Clans at 100 | 3.7 | |
| R13 | The admin UI at 100 | 3.7 | |
| R14 | Join flow at 100 (no overshoot while many load at once) | 3.6 | |
| R15 | Queueing when full | 3.6 | |
| R16 | World-store write batching (no save spikes) | 3.8 | |
| R17 | Measure what can be measured; write the limits honestly | 4, 6 | |
| R18 | Research named games and UE5 replication at scale (cook step 1) | 1 | this doc |
| R19 | Plan before building (cook step 2) | 3, 5 | this doc |
| R20 | Spot-check the build against plan and BAR (cook step 4) | 7 | |
| R21 | Read cookpass-multiplayer's work (it set the default cap 32) | 2 | |
| R22 | Data-driven numbers (tuning.json), UI with mouse AND pad | all | |
| R23 | git pull, push to master, CHANGELOG Unreleased + HANDOFF, LANDED lines; no release, no email | 8 | |

Derived (found while reading the code; each is a hole that only shows at 100):
- D1 Effects (`WRFx::Broadcast`) go out as **reliable multicasts on an always-relevant relay**: every hit spark anywhere
  on the map reaches all 100 clients. At 100 that is O(N^2) reliable traffic and a reliable-buffer overflow risk.
- D2 `AWRPlayerState` replicates at **100 Hz** (GAS) and is always relevant: 100 player states x 100 connections every
  frame is the classic 100-player wall (Fortnite's reason for the player-state frequency limiter).
- D3 The autosave serialises **every** player in one frame every 10 s (`SaveAllPlayers`: the dirty test is always true)
  and the world document in one frame every 60 s: a hitch that grows with players and pieces (Rust's and ARK's famous
  save freezes).
- D4 Global chat flood control: **already built** (token bucket `chat.perSecond` 1 / `chat.burst` 5,
  `WRPlayerController.cpp:152-156`); nothing to add.
- D5 "X is back / X left" lines go to everybody: at 100 that is a wall of joins in chat.

## 1. Research

### 1.1 How survival games run big servers

| Game | Players | How it holds up | What we take |
|---|---|---|---|
| **Fortnite** (UE4/5, the source of the Replication Graph) | 100 | Epic built the **Replication Graph** for it (Unreal Fest 2018 talk; Epic docs "Replication Graph"): a 2D **grid** of cells (actors bucketed once, each connection gathers only the cells near it), an **always-relevant** list, per-connection lists, a **player-state frequency limiter** (other players' states trickle a few per frame), **dormancy** for the building pieces (a wall stays dormant until it is hit), 30 Hz server tick. Per-actor cost is paid once per frame, not once per connection | Replication Graph with grid, always-relevant, per-connection, a people node for player states, dormant build pieces, 30 Hz |
| **Rust** (Unity, own netcode) | 100-300+ on big servers | **Network groups**: the map is a grid; you subscribe to the cells around you and get only their entities. Server tick 30 by default; the **world save** (every 10 min by default) is the well-known lag spike on big servers. Full servers show a **queue with your position** while connecting; queue-skip for admins/VIP is a server plugin | Grid relevancy; the save must not be one frame; a visible queue with position, admins skip it |
| **ARK** (UE4; ASA on UE5) | 70 official, up to ~127 community | Unreal's per-actor relevancy (NetCullDistance per class), structures are thousands of replicated actors, the big **save freeze** every 15 min on large maps; idle tames by the hundred at bases are the known server killer | Per-class cull distances and rates as data; structures dormant; idle tames drop to a slow rate |
| **Conan Exiles** (UE4) | 40 default, 70 max | Same UE4 per-actor model; official PvP servers run 40 because of building/thrall load | 100 needs more than the default driver: the graph + dormancy |
| **Valheim** (Unity) | 10 (hard cap) | Peer authority: the client that owns a zone simulates it; the server mostly relays | Shows the opposite choice; we keep server authority (Raven's rule) |
| **DayZ** (Enfusion) | 60 default, up to 127 | A **login queue** in `serverDZ.cfg`: `loginQueueConcurrentPlayers` (how many load in at once) and `loginQueueMaxPlayers` (how many may wait); a ~1 km network bubble | Admit joiners in order, hold their slot while they load, cap the waiting list |
| **PUBG** (UE4) | 100 | Ran the default driver at launch and fought server frame time for a year; moved to custom relevancy and lower rates far away | Don't ship 100 on the default driver |

### 1.2 UE5 replication at 100 players (what the engine gives us, read from the 5.8 headers/source)
- **Replication Graph** (`Plugins/Runtime/ReplicationGraph`, prebuilt in the launcher engine for editor and game
  targets): `UReplicationGraphNode_GridSpatialization2D` (cell size, spatial bias, static/dynamic/dormancy-driven
  actors), `UReplicationGraphNode_ActorList` (always relevant), `UReplicationGraphNode_AlwaysRelevant_ForConnection`
  (the viewer and its view target; owner-only actors), `FGlobalActorReplicationInfo.Settings` (per-actor period in
  frames and cull distance, copied from the class at add time). Caveats found in the source: it **ignores
  `IsNetRelevantFor` overrides** (AWRFold has one), **doesn't support `bNetTemporary`** (none in the project), **reads
  the class defaults** for rate and cull (instance changes made after spawn are ignored: `SetNetUpdateFrequency` only
  broadcasts `OnNetUpdateFrequencyChanged`, which the graph doesn't listen to; `SetNetCullDistanceSquared` notifies
  nothing), and an actor spawned **deferred** is added at `SpawnActor`, before `InitPiece` sets `bAlwaysRelevant`.
- **Iris** (Epic's newer system): filtering + prioritisation with its own grid filter, but every custom `NetSerialize`
  struct needs an Iris NetSerializer or falls back to a slow last-resort path, GAS on Iris is newer, and nothing here
  can be played to find what breaks. **Choice: the Replication Graph** (proven at 100 in Fortnite, drop-in through
  `UReplicationDriver::CreateReplicationDriverDelegate`, and switchable off per server).
- **Dormancy**: `DORM_DormantAll` + `FlushNetDormancy` replicates a change once and goes dormant again; new connections
  still get the actor's state when it becomes relevant. The project has no dormancy anywhere and ~250 places that
  write a build piece's replicated fields, so flushing by hand at every write isn't safe.
- **Defaults**: `NetServerMaxTickRate=30`, `MaxClientRate=MaxInternetClientRate=100000` B/s (BaseEngine.ini);
  `AActor` default `NetUpdateFrequency` 100 Hz and cull 150 m; the dynamic-bandwidth code in `AGameNetworkManager`
  is never called in 5.8 (no caller of `UpdateNetSpeeds`).
- **Bandwidth per client** (Epic's guidance and Fortnite talks): 15-30 KB/s typical in play, bursts to the 100 KB/s cap
  on join and in big fights. 100 clients at 25 KB/s = 2.5 MB/s (20 Mbit/s) up from the server; at the cap, 80 Mbit/s.
  A home connection can't host 100; a rented box can.

### 1.3 Real-world reference
- Queues: a restaurant waitlist holds your place only while you're there and answers "you're 3rd"; when a table frees
  the first in line gets it for a few minutes, then it goes to the next. That is the ticket rule below.
- Moderation of big rooms (IRC, Discord at scale): joins/leaves are hidden above a size, flood control is a token
  bucket (a short burst, then a steady rate).

## 2. What exists (cookpass-multiplayer, read in code)
- `WRAccess` (`Saved/Worlds/<id>/access.json`): `maxPlayers`, password, admins, bans; `Refuse` in PreLogin: banned,
  full, password. Limits `net.listenMinPlayers` 2 / `net.listenMaxPlayers` 16 / `net.listenDefaultPlayers` 8 /
  `net.dedicatedDefaultPlayers` **32** / `net.dedicatedMaxPlayers` 100. `GameSession->MaxPlayers` set the same.
- The listen host's Hosting panel sets 2-16 before hosting (`WRUIWorlds.cpp:584-588`). A dedicated server has no way to
  set its cap except editing access.json by hand.
- The players list is the Clan tab's right column (`WRUIClan.cpp` `Right()`), every row with ping, Host/Admin chip,
  clan, Kick/Ban/Make admin; the whole tab rebuilds whenever anybody's name, clan or role changes.
- The menu's card shows `WR|`/`PASSWORD|` refusals and counts down Rejoin tries (6 in 60 s).
- `UWRSignificanceSubsystem` slows far creatures (tick + net rate 4 Hz); `WRStore` writes on a background thread in
  batches (good), but the game thread builds all the JSON in one frame.

## 3. Design (every gap solved)

### 3.1 The cap (R1-R4)
- `net.dedicatedDefaultPlayers` **100** (was 32). A dedicated world's cap comes, in order, from the command line
  (`-MaxPlayers=N`, the engine's own switch, or `?MaxPlayers=N` on the URL), then `access.json`, then the default.
  Always clamped to `net.listenMinPlayers..net.dedicatedMaxPlayers` (2..100).
- **Changed while running** by the host (or a Freedom35 moderator, role 2): `admin cap <n>` (server-checked, saved to
  access.json, the engine's cap set to match). Lowering below the players already in kicks nobody: new joins wait
  (queue) until the count is under it (Rust's rule). The new cap is told to admins as a notice.
- Listen hosts stay **2-16, default 8** (`net.listenMaxPlayers` 16): a home PC renders its own game and uploads to
  everyone; 16 x 25 KB/s = 400 KB/s = 3.2 Mbit/s typical, 12.8 Mbit/s at the cap, which most home uplinks carry. The
  Replication Graph is off on listen servers by default (the proven path at that size).
- The cap and the count replicate on the game state (`PlayerCap`, `bDedicatedWorld`, `Waiting`) so the players list
  can show "37 / 100" and the host's stepper.

### 3.2 The Replication Graph (R5)
`UWRReplicationGraph` (`WRNetGraph.h/.cpp`), created through `UReplicationDriver::CreateReplicationDriverDelegate` for
the **game net driver on a server** when `net.graph.mode` says so: `"dedicated"` (default: dedicated servers only),
`"always"`, `"off"`; the command line `-wrrepgraph=on|off` overrides. Nodes:
- **Grid** (`GridSpatialization2D`): cell `net.graph.cellM` 100 m, spatial bias from the world's -/+ 2 km beyond the
  island (`net.graph.biasM` 60000 m: the continent's 49 km^2 fits). Moving things (pawns, creatures, projectiles, tames,
  siege engines, anything with replicated movement) are **dynamic**; things that never move (build pieces, loot bags,
  resource nodes, rubble, eggs, places) go in **dormancy-driven static** cells.
- **Always relevant** (global list): the game state, the world/biome state actors, Hearths and claim totems
  (`bAlwaysRelevant` on the instance), the clocks.
- **Per connection**: the viewer and its view target (engine node) plus owner-only actors (`bOnlyRelevantToOwner`:
  the forest/storm/swamp marks), added once the actor has a connection.
- **People node** (per connection, ours): the connection's **own player state every frame**; other players' states
  every frame when their body is within `net.graph.peopleNearM` 150 m of the viewer (their health and statuses on the
  bars you see), else a **round robin** spread so each far one is sent every `net.graph.farPeopleSeconds` 2 s (the
  players list, clans, pings). Rosters: an `AWRFold` (portal fold) is gathered for everyone on its roster wherever
  they are (it overrides `IsNetRelevantFor`, which the graph doesn't call), within its own cull distance for the rest.
- **Settling**: an actor's route is decided on the **next replication frame** after it was added (pending list), so
  everything set during spawn (deferred `InitPiece`, `FinishSpawning`, `BeginPlay`'s `SetNetCullDistanceSquared` on
  portals) is read from the instance: always relevant or not, cull distance, rate. Removed before it settles: dropped
  from the pending list.
- **Live changes**: the graph listens to `OnNetUpdateFrequencyChanged` (the significance subsystem's far-creature rate
  and the idle-tame rate) and updates the actor's period globally and on every connection; `WRNet::SetCull(Actor, Cm)`
  does the same for cull distance.
- **Multicasts** keep the graph's own rule: on a spatial actor they reach only connections within its cull distance.

### 3.3 Dormancy for builds and items (R6, R8)
- `Data/tuning.json` `net.classes`: a row per class name (`WRBuildPiece`, `WRLootBag`, `WRResourceNode`, `WRRubble`,
  `WRLaidEgg`) with `"doze": true`. These go **dormant** (`DORM_DormantAll`) once spawned and settled.
- **The doze watcher** (`UWRNetSubsystem`, server): for each dozing actor it keeps a shadow copy of its replicated
  fields (reflection: every `CPF_Net` property of its class, `CopyCompleteValue`) and compares (`Identical`) in time
  slices: every dozing actor is looked at once every `net.doze.checkSeconds` 0.25 s, at most `net.doze.perFrame` 600 a
  frame. A change: `FlushNetDormancy` (it replicates once and dozes again) and the shadow is refreshed. This catches all
  ~250 writers without touching them.
- **Instant wakes** where a quarter second shows: a door opening/closing (`AWRBuildPiece` door use), damage to a piece
  (`WRNet::Wake`). Both also reset the shadow so the watcher doesn't wake it twice.
- A dozing actor costs the replication pass nothing; a newly arriving player still gets its full state when they come
  within its cull distance.
- Cull distances (data): build pieces 300 m (bases read on the skyline, matched to the dressing's ~250 m decoration
  range), loot bags 120 m, nodes 200 m, rubble 120 m, eggs 80 m.

### 3.4 Budgets for things that move (R7, R9, R10)
`net.classes` rows (a row matches the class or any subclass; the nearest row wins; a field left out keeps the class's
own value; a value set on the instance in code that differs from its class default is never overridden):
| Row | Hz | Min Hz | Cull | Priority | Why |
|---|---|---|---|---|---|
| WRCharacter (players) | 60 | - | 250 m | 3 | bows reach 60-80 m and PvP needs bodies early; movement smooth at 60 |
| WRCreature | 20 | - | 150 m | 2 | the pack you see; movement is smoothed by the CMC |
| WRBoss | 30 | - | 300 m | 3 | seen across its arena |
| WRGarrison | 20 | - | 250 m | 2 | keeps its 250 m |
| WRProjectile | 30 | - | 200 m | 2 | an arrow is seen from where it can hit you |
| WRSiegeEngine | 10 | - | 250 m | 2 | big and slow |
- **Idle tames**: a tame on Stay (or any creature with no player controller) that isn't moving and isn't fighting drops
  to `net.creatureIdleHz` 4 Hz in `UWRSignificanceSubsystem::ApplyRate` (ARK's idle-base load); moving or hit, it's
  back to its row's rate on the next slice.
- **Server tick** 30 Hz (`NetServerMaxTickRate`, Fortnite's), client rate cap 100 KB/s (engine default, kept) set
  explicitly in DefaultEngine.ini with the reason.
- **Effects** (D1): `WRFx::Broadcast/BroadcastOn` send a client RPC to each player whose view is within
  `net.fxHearM` 200 m of the effect (reliable, as before), not a map-wide multicast. A listen host's own effects play
  locally as before.

### 3.5 Server budgets written as data
`net.*` in tuning.json holds every number above; the graph and the subsystem log one line a minute on a dedicated
server: players, actors in the graph, dozing actors, wakes in the last minute, frame time
(`NET 87/100 players, 23,410 actors (18,950 dozing, 312 woke), frame 21.4 ms`).

### 3.6 Join flow and the queue (R14, R15)
- **Slots held while loading**: a join that passes PreLogin holds a **reservation** (by name) for
  `net.queue.holdSeconds` 120 s, until it logs in or the time runs out. The count of "who's in" is players +
  unexpired reservations, so ten people loading at once can't push 99 to 109. `Login` checks again (a late joiner
  with no reservation over the cap is refused with the queue line).
- **The queue** (a world is full): the refusal carries `QUEUE|<pos>|<count>|<max>`. The server keeps the waiting list
  in order (name, address, last seen). A waiting joiner must try again within `net.queue.staleSeconds` 30 s or loses
  their place. When a slot frees, only the first in line may take it (positions 1..free); everyone else is told their
  position. The list is capped at `net.queue.maxWaiting` 200 (DayZ's `loginQueueMaxPlayers`); past it: "The queue is
  full too".
- **Admins skip it**: the host role and admins named in access.json come in past the cap, up to
  `net.queue.adminSlots` 2 extra (the engine's own cap is set to cap + 2 so it doesn't refuse them).
- **The joiner's card**: title "World full", line "You're 3rd in the queue (5 waiting, 100/100 playing). Your place
  is kept while this card is open.", countdown "Trying again in 8 s" (`net.queue.retrySeconds` 8), buttons **Leave
  the queue** (closes the card) and **Try now**. No try limit while queued (it's not a host that's down). Mouse, keys
  (Esc leaves), pad (D-pad to the buttons, B leaves) through the card's existing nav root.
- Admins see "5 waiting" next to the count in the players list.

### 3.7 Players, clans and admin at 100 (R11-R13)
Clan tab right column, rebuilt for 100:
- Header: **Players in this game 37 / 100** (+ "5 waiting" for admins). The host (role 2) gets a **cap stepper**
  (- / +, D-pad left/right on the pad) on a dedicated world, sending `admin cap n`.
- A **search box** (name or clan) that stays put while the list updates (it lives outside the rebuilt part), with
  **filter chips**: Everyone / My clan / Staff. "Showing 12 of 99".
- The list in its **own scroll box** (fills the column), sorted: your clanmates, staff, then by name; a joiner or
  leaver rebuilds only the list (scroll position kept), not the whole tab.
- "Clanmates share" moves to the bottom of the left column (it's about your clan), so the list has the right column.
- Lookups by map (no O(N^2) per rebuild). Banned (admins) stays under the list.
- Clans: clan size limits stay the preset's; `FWRClans::RefreshPlayerStates` is linear. Clan members' rows are
  pinned to the top, so your clan reads at 100.
- **Arrivals** (D5): "Mira is back / left" goes to everyone up to `net.arrivalsToAllUpTo` 24 players; above that only
  to her clan and admins.
- **Chat flood control** (D4): already in the build (`WRPlayerController.cpp:152`), kept as is.

### 3.8 Saves at 100 (R16)
- **Players**: the autosave runs every second and saves the players whose turn it is (round robin), so each player is
  still saved every `net.save.playerEverySeconds` 10 s but never all in one frame. The always-true dirty test is fixed
  to "every player on their turn" (inventory has no dirty flag; the turn spreads it instead). Logout and shutdown save
  at once, as before.
- **World**: the periodic 60 s save is **sliced**: frame 1 takes the list of pieces; each frame turns
  `net.save.piecesPerFrame` 1500 into JSON; then tames/eggs/clock and the write (to the store's writer thread, as now).
  A piece gone mid-save is skipped; one placed mid-save goes into the next save. Shutdown, the admin save and logout
  paths call the immediate save (and cancel a running sliced one).

### 3.9 Edge cases
| Case | Behaviour |
|---|---|
| Cap lowered below players in | nobody kicked; joins queue until count < cap |
| Cap raised while people wait | the first N in the queue get in on their next try |
| Queued player's PC sleeps | stale after 30 s, place lost; the card says so on the next try ("you're 9th") |
| Two joins for the same name | the reservation is by name: the second is refused by identity as before |
| Reservation holder never logs in | released after 120 s |
| Admin joins a full world | in, up to cap + 2 |
| Graph off (`-wrrepgraph=off`) | the default driver with the same per-class budgets and dozing |
| Listen server | default driver; dozing and budgets still apply |
| Portal fold roster member far away | gathered by the people node, cull zeroed for them only |
| Hearth/totem set always relevant in InitPiece | settled next frame, so it's routed always relevant |
| An actor's rate changed at runtime | graph updates its period (global + every connection) |
| A dozing piece written by any of ~250 sites | watcher wakes it within 0.25 s; doors and damage at once |
| Effect at a place nobody is near | sent to nobody |
| 100 players all within 150 m (a raid) | all player states every frame to all: the worst case, bounded by the 100 KB/s client cap; written in the limits |

## 4. Measuring and limits (R17)
No agent tests are allowed (Aaron 11-07, 11-12), so nothing is measured here: the gate is the compile. The numbers
below are estimates from the engine's defaults and the research, and the log line in 3.5 is how Aaron's test server
will measure them. Written into HANDOFF as limits:
- Server upload: ~2.5 MB/s (20 Mbit/s) typical at 100, up to 10 MB/s (80 Mbit/s) worst case. **100 needs a rented or
  wired box, not a home connection.**
- Dedicated servers still run as the editor binary with `-server` (a packaged dedicated server needs Unreal built from
  source: `Tools\build_engine.ps1`; `WildreachServer.Target.cs` is added so that build is one command).
- All 100 in one 150 m fight: every player state every frame; client bandwidth caps out and far things update slower.
- AI: creatures still think on the server for every active wildlife site near any player; 100 players spread over the
  map wake more sites; the far-creature slicing holds, but the total AI budget is not capped.

## 5. Build steps
1. tuning.json `net` block (+ `chat`), DefaultEngine.ini net section, Build.cs + uproject: ReplicationGraph.
2. `WRNet.h/.cpp`: budget table, `ApplyBudget`, `Wake`, `SetCull`, `UWRNetSubsystem` (spawn hook, doze watcher, log).
3. `WRNetGraph.h/.cpp`: the graph, the people node, settling, live rate/cull changes; module startup binds the delegate.
4. Significance: idle-tame rate. Pieces: Wake on door and damage.
5. WRFx: per-player effect RPCs on `AWRPlayerController`.
6. Access: default 100, `-MaxPlayers`, reservations, queue, admin slots, `admin cap`; game state `PlayerCap/Waiting`.
7. Menu card: queue state.
8. Clan tab: header, cap stepper, search, filters, list scroll, sort; left column gets "Clanmates share".
9. Arrivals rule, chat token bucket.
10. Saves: round-robin players, sliced world save.
11. `WildreachServer.Target.cs`.
12. Compile; spot-check; CHANGELOG + HANDOFF; commit; push; LANDED lines.

## 6. Spot-check notes (read against this doc and THE BAR after building)
- **Turrets** (found reading `WRTurret.cpp`): a tracking turret slews its head every frame, so dozing would have cut
  its aim to the watcher's 4 updates a second. Fixed: `WRNet::KeepAwake` keeps a dozing actor awake while it tracks
  and lets it doze when it stops (`WRTurret.cpp:276/299/304`); the watcher skips awake ones. Lifts and belts are
  time-stamped (clients work out the motion), so they doze fine.
- **Idle creatures**: first draft slowed every idle creature near players; a sleeping wild beast's first lunge could
  have gone out up to half a second late. Narrowed to **tames** (the ARK base problem) and a rate rise now forces an
  update at once (`WRSignificance.cpp:106-115`).
- **Build pieces' awake rate** is 10 Hz (the row), not 2: only the moments between a wake and dozing again use it.
- **Clan tab**: the filter buttons show which is on, so changing it redraws the tab (search text survives: the box is
  a member); a join/leave redraws only the list, scroll position kept.
- **Ghost pieces** (`SetReplicates(false)` after a deferred spawn) never settle in the graph or doze (both check
  `GetIsReplicated` when they act).
- **Old access.json files** written with the old default 32 (an admin banned someone) come up at 100 on a dedicated
  server: `capSet` marks a chosen cap; the Hosting page and `admin cap` set it.
- Compile: `Build.bat WildreachEditor Win64 Development` succeeded, no warnings (3 runs, the last after the fixes).

## 7. Ledger proof
| # | Done | Where |
|---|---|---|
| R1 | yes | everything below; cap limit `net.dedicatedMaxPlayers` 100 (`Data/tuning.json:124`) |
| R2 | yes | `"dedicatedDefaultPlayers": 100` (`Data/tuning.json:124`); `WRAccess::Cap` (`WRAccess.cpp:321`) |
| R3 | yes | `-MaxPlayers=` / `?MaxPlayers=` (`WRAccess.cpp:333`), `admin cap` (`WRAccess.cpp:424`), host stepper (`WRUIClan.cpp:502`), Hosting page marks `capSet` (`WRUIWorlds.cpp:584`) |
| R4 | yes | listen 2-16, default 8 kept (`Data/tuning.json:124`); graph off on listen (`WRNet.cpp:144`) |
| R5 | yes | `UWRReplicationGraph` (`WRNetGraph.cpp:159` nodes, `:228` settle, `:322` frame, `:357` live rates), factory (`Wildreach.cpp:16`, `WRNet.cpp:144`), plugin (`Wildreach.uproject`, `Wildreach.Build.cs:41`) |
| R6 | yes | `doze` rows (`Data/tuning.json:127`), watcher (`WRNet.cpp:280` Doze, `:300` Watch), wakes (`WRBuilding.cpp:705/721/1290`), turrets (`WRTurret.cpp:304`), graph dormancy cells (`WRNetGraph.cpp:228`) |
| R7 | yes | `WRCreature`/`WRBoss`/`WRGarrison` rows; idle tames (`WRSignificance.cpp:106`); far rate reaches the graph (`WRNetGraph.cpp:357`) |
| R8 | yes | `WRLootBag`, `WRResourceNode`, `WRRubble`, `WRLaidEgg` doze rows (`Data/tuning.json:127`) |
| R9 | yes | `WRProjectile` row (30 Hz, 200 m); effects only to players within 200 m (`WRFx.cpp:157`, `:1011`) |
| R10 | yes | `net.*` budgets (`Data/tuning.json:124-150`), 30 Hz tick + 100 KB/s (`Config/DefaultEngine.ini:87`), grid cells (`WRNetGraph.cpp:159`), people node (`WRNetGraph.cpp:15`), NET log line (`WRNet.cpp` Tick) |
| R11 | yes | count / cap / waiting header, search, filters, own scroll, sort (`WRUIClan.cpp:163`, `:540`, `:573`) |
| R12 | yes | clanmates pinned first, My clan filter (`WRUIClan.cpp:573`); "Clanmates share" moved left (`WRUIClan.cpp:448`); arrivals to clan + admins above 24 (`WRGameMode.cpp:834`) |
| R13 | yes | Kick/Ban/Make admin rows kept in the new list, Staff filter, waiting count for admins, host cap stepper (`WRUIClan.cpp:502`, `:540`) |
| R14 | yes | held slots while loading (`WRAccess.cpp:317`), Login re-check (`WRGameMode.cpp:141`, `WRAccess.cpp:367`), engine cap = cap + admin slots (`WRAccess.cpp:344`) |
| R15 | yes | the queue (`WRAccess.cpp:291`), admin slots, queue full; the menu card (`WRMainMenu.cpp:1488`, `:603`) |
| R16 | yes | round-robin autosave (`WRGameMode.cpp:792`), sliced world save (`WRGameMode.cpp:420`, `:438`); store writer thread as before |
| R17 | yes | section 4 (limits, estimates, not measured: no tests allowed); HANDOFF "100 players" |
| R18 | yes | section 1 |
| R19 | yes | sections 3, 5 |
| R20 | yes | section 6 |
| R21 | yes | section 2 |
| R22 | yes | all numbers in `net.*`; Clan tab controls are SWRButtons / a text box on the journal's nav (mouse, keys, pad) |
| R23 | yes | commit on master, CHANGELOG + HANDOFF in it, LANDED lines |
