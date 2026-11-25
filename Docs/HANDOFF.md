# Wildreach handoff: working on the game from another PC

For a second developer (and their Claude) joining Raven on Wildreach. Written 2026-11-08, at v0.7.1. Keep it current:
when something here stops being true, fix it in the same commit.

Setting up the PC is in `README.md` ("Working on the game on a new PC"), and `Tools\check_setup.ps1` checks it.
This file is what comes after: where the game stands, how two people share the repo, how a release goes out, and the
traps that already cost a day each.

## Who decides what

- **Raven owns the game.** Scope, names, looks and what goes out are Raven's call. Ask before building anything big,
  and before publishing a release.
- **Scope** (Aaron, 2026-11-13): the full game, all 12 lands, built land by land; Storm peaks (Lightning) is the first
  off the Old Island.
- **The design docs are first drafts.** Names, perks and starting numbers are meant to be tuned in playtests, so
  numbers live in data (`Data/*.json`, `Data/tuning.json`), never hard-coded around one value. Where the doc's tabs
  disagree, `Docs/DESIGN_REVIEW.md` lists the open questions and a proposed default: use the default, leave it open
  for Raven.
- **Creature looks:** a remade creature keeps the design doc's fantasy and is realistic only in anatomy and pose; four
  legs unless the real animal it's based on has more.

## The design docs

Raven writes them in Claude Docs. Copies as of 2026-11-08 are in `Docs/design/` (`build-plan.md`, `design-doc.md` and
one file per tab). Raven keeps editing the live ones, so the copies go stale: ask Raven for a fresh export before a
big piece of work. The build plan's "Where it stands" table says what's built.

## Where the game stands (v0.8.1, the full game: all 12 lands)

Every system in the First playable tier is built and has a bot test: biomes, both paths, levelling, combat, six
bosses, enemies and wildlife regions, taming, survival, crafting (with time and station queues), building (with
claims, raids and door keypads), clans, PvP rules, worlds and world settings, the stone-and-leather UI, sound, the
installer and auto-updater. `CHANGELOG.md` has every version, newest first, with what changed and why.

0.8.2 (lane wild-f35): players get the Shipping build (184 MB exe, no `.pdb`), the cooked content is cut
to what the game uses and 2K textures (3.1 GB on disk, was 4.3 GB) in files of up to 60 MB, and Wildreach is listed on
Freedom35 with a small installer that downloads the files; a Freedom35 install updates only through Freedom35
("Releasing on Freedom35" below). Live there: 0.8.2.2 (2026-11-17), which also fixes every quit ending in heap
corruption (static Slate widgets freed after ICU shut down; `SWRMainMenu::ReleaseAtExit`). The same game is the GitHub release v0.8.2 (2026-11-17, packaged on Raven's PC
without the Electric Dreams kit); from now on every version goes out on both (Raven 11-17, "Releasing an update").
The unit and bot tests guard again since phase 2 (11-19, CHANGELOG "Unreleased"): all 39 suites pass on one run of
`Toolsalltests.ps1` (about 2 hours; `-Only a,b` reruns a few).

In 0.8.1: every screen fits every screen (`WRUIFit.h`: a 1760 x 1080 minimum canvas and `SWRFit` around every viewport
root; the main menu laid out by flow, the survivor framed into its free stage), Update now never opens a web page, and
every package carries its updater. The Ancient forest still shipped its tree-kit look: the packaging PC (the desktop)
has no Electric Dreams kit yet. Also since 0.8.0: **the NPC strongholds are Elden-Ring-style forts with a garrison** (lane
build-wild-strongholds, cooks/build-wild-strongholds.md in showcase-notes): the layout is data
(`Tools/terrain/strongholds.mjs` -> `Data/strongholds.json`: outpost, gate front, breach, yard, keep in the land's set,
inner ward + postern + run-back stone; ladders are fort-owned `AWRSiegeLadder`s), the soldiers are `AWRGarrison`
(`WRGarrison.cpp`: the hero body pressing the survivor's own ability inputs, so `weapons.json` move sets are theirs;
ranks and warlords in `strongholds.json`), and `UWRStrongholdSubsystem` (`WRStronghold.cpp`) keeps the fort: alarm,
respawn while standing, the warlord's fall takes it, rebuild after `rebuildHours` with nobody near. A new soldier or
warlord is a data row; a new fort is a `forts` entry in the script (siting, layout kit, garrison). Soldiers steer
straight with a sidestep (no navmesh), so keep posts and routes on open ground.

Released in 0.8.0 (2026-11-16) with everything below: **F8 reports** (testers' reports land in the studio inbox, in `Saved/Reports` on their PC
and, from a client, in the host's `Saved/Reports`), **full controller support** (`Data/gamepad.json`; menus driven by
one input processor in `WRPadInput.cpp`, so a new screen needs nothing for the pad as long as its clickable pieces
show the hand cursor or are buttons / slots), the **F1 controls page**, and **look sensitivity / invert**. New prompt
rule: show a key with `WRInput::ActionKey(<action>)`, not `WRUI::KeyName(...Key(...))`, so pads see their buttons.
Also since 0.7.1: **the UI cook pass** (cooks/cookpass-ui.md in showcase-notes): player prefs beyond keys are one table
in `FWRKeyBindings` (`Pref` / `SetPref`, ids listed in WRKeyBindings.h): read them where they act, never copy them.
Settings has Camera (FOV per view, shake, blur, shoulder) and Interface (Desk / Couch, text and HUD size, HUD mode,
compass, names, helmet, crosshair, damage numbers, hints) pages; Look and controller has aim speeds, dead zone,
response, vibration strength and hold / toggle for sprint, guard and aim (`AWRCharacter::InputSprint`, `InputPressed`).
The HUD's Full / Fade / Minimal modes are `SWRHud::TickHudMode` (WRUIHud.cpp): a new HUD panel that should fade joins its
lists there; anything that asks for action (prompts, notices, boss bars) stays out of them.
Network failures go to the main menu's card (`SWRMainMenu::NoteFailure`, plain lines, Rejoin for a joined game);
a crashed run's report is offered once on the next start (`WRReport::PendingCrash`, stamps in `Saved/Wildreach`).
Performance cook pass (cooks/cookpass-performance.md): `WRPerf.cpp` owns the player's machine: the first launch's
quality pick (benchmark + graphics-memory caps, `perf.vramTiers`; Auto-detect is the same call), the texture pool sized
to the card (`perf.texturePool`), the Video page's graphics memory meter and the frame time readout (`wr.perfoverlay`),
and the shader warm-up (`UWRPerfWarmup`: every surface material the preload brings drawn once behind the black cover,
because procedural meshes get no PSO precaching). The loading cover (`FWRFlow`) now waits for `WRPerf::Ready` (preload,
warm-up, no shaders compiling) and shows `WRPerf::Stage`. A new `LoadObject(TEXT("/Game/..."))` path: re-run
`node Tools/perf/scan_code_assets.mjs` so it's preloaded (and warmed, if a material).
Text boxes need nothing for the pad: A over one opens the pad keyboard (`WRUIPadKeyboard.cpp`); a screen that
wants its own Done / Cancel calls `WRPadKeyboard::Open` with a target (the chat line does).
The main menu's look picker previews on the menu world's own survivor (`AWRCharacter::PreviewLook`, standalone only).
Map pins live on each PC per world (`WRUIMap::GetPins`, `Saved/Wildreach/Pins`) and are shared with the clan
(`WRClanPins`, Aaron 11-16): a changed list goes up (`ServerMapPins`, at most once a second), the server keeps everyone's
in the world doc `map_pins` and sends each player their clanmates' (`ClientClanPins`) on a change, a join or a clan change
(`FWRClans::RefreshPlayerStates` calls `WRClanPins::SendAll`). They show on the map and compass only with the legend's
"Clan pins" ticked (off by default, `[Wildreach.Map] ShowClanPins` in GameUserSettings.ini).
The pad's D-pad right: a tap is the next ability, a hold the item wheel (`FWRPlayerUI::ItemWheelKey`, the wheel's mode 3).
The tracked recipe is `FWRPlayerUI::SetTrackedRecipe / TickTracked` and the HUD's `TrackedCard`.
The fill-in map is `UWRMapRevealComponent` (PlayerState, server-side grid, `mapReveal` in the player save); a new map
marker for a place (not yours) should check `IsRevealed` in `WRUIMap` Gather like the camps and caves do.
A worn set's full bonus plays the set-complete moment (`FWRPlayerUI::TickSetComplete`, sounds.json `gear.setComplete`).
Path node glyphs come from `Tools/ui_glyphs.mjs` (Necromancy's added): run it with the ids you add so the rest stay as they are.
**Multiplayer cook pass** (cooks/cookpass-multiplayer.md in showcase-notes): doors swing on their hinge from server-set
`bOpen` + `DoorSide` (`AWRBuildPiece::OnRep_Door` starts the swing, `Tick` runs only while it moves, `door.*` tuning);
open or shut a door for a person with `SwingFor(Pawn, bOpen, Why)` (away from them, refused with a reason while a body
is in the swing), and without one with `SetOpen` (blasts, build copy). Don't set `bOpen` directly.
Offline raid protection is `FWRSocialRules::ProtectionFor(World, Owner, LastAttackedUtc)` (arming and quiet timers;
leaving times noted in `AWRGameMode::Logout`); a claim keeps its state in `AWRClaimTotem::ProtectState` /
`ProtectArmsAt` (updated each second) and its last attack in `LastAttackedUtc`. Damage to pieces goes through
`TakeBuildDamage`, which applies it; don't check `IsAnyoneOnline` for protection anywhere else.
Chat lines carry their channel at the start (`/g /l /c /w name /r`, parsed in `ServerChat`); `ClientChat(From, Line,
WRChat::EChannel)` delivers one. A system line from the world uses channel `WRChat::System`.
Who may join is `WRAccess` (`access.json` per world: cap, salted password hash, admins, bans); `PreLogin` calls
`WRAccess::Refuse`, whose reasons start `WR|` or `PASSWORD|` so the main menu's card can show them (and a password box).
Roles are `AWRPlayerState::AccessRole` (2 host, 1 admin), refreshed by `WRAccess::RefreshRoles`; admin actions are the
`admin kick|ban|unban|op|deop` server command. Leaving reasons for the card: `SWRMainMenu::NoteLeaving` (kicks, the host
closing: `AWRPlayerController::AnnounceHostClosing`).
Dropped bags have a kind (`AWRLootBag::KindDeath / KindSpill / KindDrop`, set with `Setup(kind, life)` or `Spill(..., kind)`)
that picks their modelled look (`WRLootLook`); a death's bags are saved in the world doc `bags` (`AWRLootBag::SaveNow`).
Wake points: beds, the player's clan's unbroken Hearths (`WRBeds` HearthsFor / SpotByHearth; kind "hearth"), camps,
shrine-stones. A claim's name is its totem piece's `CustomName` (`AWRClaimTotem::ClaimName`). A world's backups can be
restored from the menu (`WRStore::RestoreBackup`), never while it runs.
**100 players on a server** (lane build-wild-100players; cooks/build-wild-100players.md in showcase-notes): a dedicated
server's cap defaults to 100 (`net.dedicatedDefaultPlayers`; `-MaxPlayers=N` for one run; `admin cap N` or the host's
stepper on the Clan tab while it runs, saved with `capSet` in access.json); listen worlds stay 2-16. Dedicated servers
run the Replication Graph (`UWRReplicationGraph`, `WRNetGraph.cpp`, made by `WRNet::InstallGraphFactory`;
`net.graph.mode` dedicated/always/off, `-wrrepgraph=on|off`): a 100 m grid, always-relevant list, per-connection
owner-only actors, and a people node (your own player state every frame, people within 150 m every frame, the rest
every 2 s; portal fold rosters). Rules for new code: an actor's route is read on the replication frame after it spawns,
so set `bAlwaysRelevant` / cull distance during spawn or `BeginPlay`; change a cull distance later with `WRNet::SetCull`
(rate changes through `SetNetUpdateFrequency` reach the graph by themselves); don't override `IsNetRelevantFor` (the
graph doesn't call it: add a node). Per-class budgets are `net.classes` rows (`WRNet::BudgetFor/ApplyBudget`).
Build pieces, loot bags, resource nodes, rubble and laid eggs **doze** (`DORM_DormantAll`): the server's watcher
(`UWRNetSubsystem`) compares their replicated fields every 0.25 s and wakes them on a change, so writing a field is
enough; call `WRNet::Wake(Actor)` where a change must show this frame (doors, damage do) and `WRNet::KeepAwake(Actor,
bool)` for a stretch of per-frame change (a tracking turret). Server effects (`WRFx::Broadcast/BroadcastOn`) go to the
players within `net.fxHearM` (`AWRPlayerController::ClientFxAt/On`), not to everyone. A full world queues joiners
(`WRAccess::Refuse` answers `QUEUE|pos|waiting|playing|cap`; the menu card keeps the place and asks every 8 s); a slot
is held for a joiner while they load (`net.queue.holdSeconds`), admins skip the queue into 2 extra slots. Autosave
saves a share of the players each second; the 60 s world save is sliced (`net.save.piecesPerFrame`); shutdown and the
admin's save are whole (`SaveWorld`). The server logs `NET <players>, <clients>, graph routes N actors, N dozing (N
woke this minute), frame N ms` once a minute: that's the line to read on a full server.
**Limits (not measured: no agent tests):** ~2.5 MB/s (20 Mbit/s) up from the server at 100 typical, up to 10 MB/s in a
crowd (each client is capped at 100 KB/s): 100 needs a rented or wired box, not a home line. A dedicated server still
runs as the editor binary with `-server` (`Source/WildreachServer.Target.cs` is ready for a source-built engine). All
100 in one 150 m fight sends every player state every frame to everyone and saturates the clients' 100 KB/s. Creature
brains aren't capped in total: 100 players spread out wake more wildlife sites.
Also in 0.8.0: **the audio cook pass** (cooks/cookpass-audio.md). The music is always on (breaths between pieces,
`music.gap`); every duck is capped at 4 dB in `UWRAudioDirector::Duck`; mode changes wait for the playing piece's beat or
bar (give a new pool row its `bpm`); combat music needs `music.combatThreat` worth of creatures on you (or a PvP blow);
a boss's fall plays `music.sting.boss_fall` and a planned hush. Reverb comes from sounds.json `spaces`, picked in
`UWRAudioDirector::SpaceNow` (add a new hall or cave there). Spot sounds and sleep breaths are rows (`ambient.spots`,
`creature.<size>.sleep`); tells can `rise`. Captions: give an event row a `caption` (and `danger`), or a creature call kind
a `captionDefaults` entry; the HUD draws them (`WRUICaptions.cpp`). The master limiter / Night compressor is
`UWRAudioSubsystem::ApplyDynamics`.
Fire and Frost have their own music sets (`explore/night/combat/mini/camp.volcanic|tundra`, `boss.vulkarn|skaldrith.p1-3`,
`sub.kilnroads|bluebelow` (`sub.<tube id>`), `layer.eruption|whiteout`); the other new lands have their own explore / night / combat, their
boss pools are still marked stand-ins. Sources and the build: `Tools/audio/sources_cookpass.py`, `build_cookpass_audio.py`.
Also since 0.7.1: **the Fire and Frost world cook pass** (showcase-notes cooks/cookpass-world.md). The Old Island's places
are data: `Data/island_places.json` is made by `node Tools/terrain/island_places.mjs` (hand places in metres; since
answers 11-16 the ones whose fire-frost.md spot didn't fit the island are found by the landform their story needs, the
tool's "site rules": `headland`, `hollow`, `overlook` (sight line), `onRoad`, `apron`, `lowland`, `bank`, `nearCoast`,
`onLakeIce`, `iceLow`, `flatDisc`, `ravine`, `fleeing`, `below`; every row carries `why`. A new place picks a rule, not a
settle radius. The rest of the file: the two waymark roads, the hazard spots, and ~180 kit POIs dealt so no route goes 250 m
without a place). Re-run it when the island or `world.json`'s villages / shrines move. `UWRIslandPlaces`
(WRIslandPlaces.cpp) builds every place on every machine once the ground under it streams in (no map change, no
replication); the looks are `WRIslandPlacesLook.cpp` (Volcanic), `WRIslandPlacesLook2.cpp` (Tundra) and
`WRIslandPlacesKit.cpp`; a new kind is a builder there and a row in the tool. Sub-zones (V1-V5, T1-T5) are
`WRIsle::ZoneAt`: their beds are `ambient.beds` layer `ffzone.<id>` and their names show on entering (`FWRFlow::CheckZones`).
Landmarks join `FWRPoi::All` as kind `landmark`. The micro-hazards (`WRIslandHazards.cpp`, tuning `hazards.ff`) run on the
server's clock; only thin ice and snow bridges replicate (`AWRIsleState`).
Also since 0.7.1: **the MetaHuman hero** (M1 Ash / F1 Wren from the hero lab, `Content/Wildreach/Characters/Hero`)
with **motion-matched locomotion** (`UWRHeroAnimInstance`), retargeted action clips with combat windows, sockets and
full-body first person on the head bone; the Mixamo survivor is gone. How it's put together is under "Art" below.
Also since 0.7.1: **fishing** (`WRFishing.cpp`, `Data/fish.json`), **swimming and engineering XP**, **66 of 120 perks
working** with the rest hidden by `waitsFor` in `Data/lifeskills.json` (remove it when a system ships), the **tame
whistle** (H / hold X), **modelled held tools** (`WRToolMesh.cpp`, `M_HeldTool`), and build pieces that collide as
they look (furniture, banner, rug; the Hearth, stations and totem fit their models via `FWRBuildLook::FitCollision`).
Also since 0.7.1: **the hero's look** (`WRHeroLook.cpp`): 12 faces and 3 builds (`hero.<sex>.faces / builds`, picked by
`?Look=<face>.<build>`; picked on the main menu's Look row (UI cook pass)), **worn armour shows** (hide, copper,
bronze, iron now; the 12 steel biome sets and their Ascendant versions are mapped by item id in `armorLooks` for the
crafting cook), body-hide masks under every piece, and **path marks** on skin, eyes and hair on every machine
(`player_art.json paths`), with Blood's canines and Lightning's crackle on cast.

Also in 0.8.0: **the testers' god mode** (cooks/build-godmode.md in showcase-notes). Only Aaron, Raven and Colin get
it, by their Freedom35 accounts: the site's `game_testers` list (freedom35-studios `node tools/testers.mjs list | add |
remove`, admin token) signs a per-connection proof (`POST /api/pc/tester`) that the server checks with the public key in
`WRGodAuth.cpp` (rotating the site's `TESTER_SIGNING_KEY` needs that key slot filled and a new build first). The game
must be run through Freedom35 (its updater's PC link) for the proof; editor runs keep the old testing commands open, a
packaged game needs a verified tester for them (`WRGod::IsDevCommand` lists them: a new testing command goes there).
A new god power is a verb in `WRGod::Run` and a row in `WRUIGod.cpp`; anything that rewards a player (a new achievement
source, a leaderboard) must skip `WRGod::IsTestedCharacter`. Since wild-answers-1115 (Aaron 11-16) a verified tester
must first type **`god-mode`** in the chat line (`WRGod::ChatLine`, `tuning god.gateWord`; typed again it all goes off,
`UWRGodComponent::bArmed`); from anyone else it is an ordinary chat line. It works on **public servers too**, where a
tester in god mode also moderates like the host (`WRAccess::RoleOf` -> 2: kick, ban, admins); `god.privatePresets` now
only decides whether the panel says the world is public.

Also in 0.8.0: **Aaron's 11-15 question answers** (lane wild-answers-1115, showcase-notes cooks/wild-answers-1115.md, which
also keeps the 20 cards' full text): god-mode gate word and mod mode (above); **Wet is off** (`tuning wet.enabled` 0: one
switch brings it all back for its design pass); **flint** comes away with mined stone (`tuning gather.flint`,
`FWRGathering::Gather`; Hush arrows and flint arrows use it); **levistone moss** floats beside every keel outcrop
(AWRSkySpot Kind "moss", `WRSkyMoss.cpp`, `tuning skyWorld.moss`; the Cloudback's favourite food, `creatures.json
foodValue`); **eating is the right hand's** (the weapon put away for the use, `WRHeldFood.cpp`; the pose in
`WRHeroAnimInstance.cpp`), from the quick-use bar (the item card names the key); **siege engines pack up on a held Interact** (`FWRPlayerUI::SiegeKey`,
server-timed) and **work on PvE worlds against the NPC strongholds** (`WRStronghold.cpp`, `Data/strongholds.json`, re-site
with `node Tools/terrain/strongholds.mjs`; their pieces are owned by `@hold:<id>`: never saved with the world's pieces,
always raidable, `FWRStrongholds::IsHoldPiece` is the test any new raid or piece rule needs); **the forest floor is
textured** (`M_ForestGround`, `Tools/terrain/build_forest_ground.py`; the mix per sub-zone and surface is `layers` in
`Tools/terrain/forest.mjs`: rerun it, then keep only `Data/lands/forest.json` if it reorders `island_dressing.json`'s POIs).
Question cards: every future card says in full sentences what the thing is, where it is in the game, what
is built, what the other choice changes and why it is a question (Aaron 11-16, r-dunewrap).

Also since 0.7.1: **the combat core** (COMBAT-AND-LESSONS C2-C5, cooks/build-combat-core.md): melee swings are move
rows in `Data/weapons.json` (`FWRWeapons`), timed the same on the server, the owner and in the warped clip; one rule set
for what can start when (`WRCombatRules`: combo and cancel windows, block stun, parry recovery) shared by the abilities
and the hero's **input buffer**; the **hit stack** by tier in `WRFxCombat.cpp` (hit-stop with creep and catch-up, victim
shake and flash, FOV punch, rumble, four sound layers); stamina grace to -20, equip load tiers for the roll, backstep,
jump legs; **guard boost / chip / block stun / guard counter**, tap **parry** with counts, **crits** (`UWRGA_Critical`);
**stance** on creatures and bosses and **poise** from armour; and the **server rewind** (`UWRLagSubsystem`): a player's
swing and crit are judged where it saw its target, and a creature's blow on a remote player waits for the defence it
pressed. New rules for new code: every blow goes through `ApplyHit` with its flags (`bParryable`, `bLow`, `Impact`,
`StanceDamage`, `Tier`); code that acts on a blow's result passes `ApplyHit`'s `OnResolved` (a held blow resolves later);
window / state tags owned by an ability go through `AWRCharacterBase::SetCombatTag`.

Also since 0.7.1: **the full combat** (COMBAT-AND-LESSONS C6-C12, cooks/build-combat-full.md): nine weapon classes with
full row sets in `Data/weapons.json` (charged, chain, running, rolling, jump, guard poke; `PickMove` reads the state),
two-handing (`bTwoHanded`), every status in `UWRStatusComponent` (`WRStatus::EId`, `status.defs`; meters replicate as
`AWRCharacterBase::BuildupPct`), element reactions in `ApplyHit` (`reactions`), the committed drink
(`UWRInventoryComponent::StartDrink`), Wildbrand rows (`wildbrands`) and spell shapes (`spells.shapes`), **creature and
boss attack rows in `Data/attacks.json`** (`AWRCreatureAI::TickRowFight` in `WRCreatureCombat.cpp`, the boss machine in
`WRBossCombat.cpp`), lock points and breakable parts (`creatures.json lockPoints / parts`), the roaming minis Cinderhorn
and Old Tusker (`creatures.json roam / props`, spawned by `AWRGameMode::StepRoamingBosses`), shrine-stones
(`AWRRunbackStone`) and first-kill drops (`AWRFirstKillDrop`, `bossKills` in the player save), the saddle
(`ServerRideAttack`, `Unhorse`), PvP multipliers and the combat tag (`State.CombatTagged`). New rules for new code: a new
creature or boss move is a row in `attacks.json`, not code; a new status is a `status.defs` row plus a case in
`UWRStatusComponent::Proc`; a new path's Wildbrand is a `wildbrands.<node>` row plus its `ClassForNode` line; anything
that fires at players from a creature or boss never homes (`AWRProjectile::HomeOn` refuses).

Also since 0.7.1: **the combat cook pass** (cooks/cookpass-combat.md in showcase-notes). A survivor's swing cuts with the
drawn weapon's **real path**: `FWRBladePaths` (`WRBladePath.cpp`) samples the hero's light / heavy clips once per clip on
whichever machine judges the blow (a dedicated server loads the body mesh and clips for this alone) and `UWRGA_Melee::Sweep`
puts the class's `weapons.json blade` row in that frame; world things (nodes, buildings, fold things, arena pieces) still use
the reach tube. A chain light into a wall recoils (`CheckRecoil`, tuning `combat.recoil`). New rule: a new weapon class needs a
`blade` row (two points in the fist's frame and a radius), and a new hero swing clip needs its `trace` window in
`anim_windows.json`, or its blade path is wrong. A one-off burst under a tick's rules sets `FWRHit::bBurst` so it gets the full hit stack;
the tier rises by itself for impact 3+ or a fifth of the body's health. A bar that should show what a blow took uses
`SWRBar` `.Trail(true)`; your running damage on a body is `UWRFxSubsystem::Tally`. A blow that isn't a body (a shockwave, a split in the ground, a bore, a sweep) is an
`AWRStrikeFront` (`Ring` / `Line` / `Arc` with the owner's hit callback), never an instant radius check; every enemy
shot gives at least `creatureAI.minWarning` from its tell. Bespoke boss moves follow the same rule (the cook pass turned every land's instant
area hits into fronts, real bodies or warned moments); a ground mark only ever shows under something really falling, from
the moment it falls. No summon counts toward a boss's health (`UpdateParticipants`).

Also since 0.7.1: **the survival cook pass** (cooks/cookpass-survival.md in showcase-notes). Food and drink are a committed
use like a potion (`UWRInventoryComponent::StartConsume`, `State.Drinking`), what eating gives is one function
(`ApplyConsumable`), and every machine shows the item in the left hand going to the mouth (`AWRCharacter::MulticastConsume`,
`WRHeldFood.cpp`). New rule: a new food, drink or potion gets a row in `Data/food_look.json` (shape and colour) or it's held
as whatever its id's words or category say. An item that leaves something when used or crafted with names it in
`items.json "returns"`; an empty container names its full kind in `"fills"` (filled at water by `UWRWaterSubsystem::DrinkHere`,
which a well and a running channel call too). The hero's procedural arm layers (seat, torch carry, eating, sleep breath)
live in `FWRHeroAnimProxy::PostLayers` and give way to one-shot clips. Kitchen stations tick on drawing machines for their
show (`WRKitchenShow.cpp`); the cooking pot's lid is the station's, not the piece's blocks. A crop plot's plants are nine
components (`PlantMeshes`) so they can sway; new crop looks still go through `FWRFarmLook::Plant` at the origin. Penned
animals' idle moods are the pen's (`AWRCreature::PennedUntil / bPenHungry`, read in `SetMood`). Wet is the survival
component's (`IsWet`), read by `ApplyHit` and `AddStatusBuildup`.

Also since 0.7.1: **the crafting cook pass** (cooks/cookpass-crafting.md in showcase-notes). Hand recipes are a timed,
committed work and the four crafting stations take the order's maker as a worker (`FWRCraftWork`, server; the replicated
`AWRCharacter::Work` drives the body, held tools and beats on every machine, `WRCraftWorkLook.cpp`; the `Work` pose layer
in `PostLayers`). A station's work spot, the piece's and the finished goods' spots are tuning (`craft.work.spot`,
`craft.show`, station frame cm, measured on the fitted props); a new station with hands-on work needs a `KindFor` kind, a
spot and its show spots. `AWRStation::SpeedHere` folds in the worker (x`craft.work.handsOn`). The forge's anvil and the
alchemy table's trestle are station components with their own collision on every machine (`BuildWorkFixtures`); the
alchemy prop now sits `craft.show.alchemy.tableTop` up. Crafting inputs count and take your nearby chests through
`FWRCrafting::Shortfall / TakeInputs (StockAt)`; pass the station (or the crafter's pawn by hand) as StockAt.

Also since 0.7.1: **survival** (FULL-GAME 6.1, cooks/build-survival.md): `UWRSurvivalComponent` on the PlayerState
owns sleep and Rested, comfort near beds, lit / unlit, the weather on that player, corruption and the held light
(`EWREquipSlot::Light`, `EquippedLight`); `UWRWeatherSubsystem` rolls `Data/weather.json` per biome into
`AWRGameState::Weather` (every machine draws and hears it from there); `UWRLightSubsystem::IsLit / IsWarded / IsHalving`
(placed lights, Hearths, glowing creatures, held lights); `WRBiome::At` is the one place that says which biome a point
is in (the island's ridge split today; the 12-wedge lookup goes there). New rules for new code: a new weather is a
`weather.json` row (effects, counters, look, glyph) plus a `weather.<kind>` bed; a new light is a `tuning lights.<id>`
row; decor gets a `pieces.json comfort` row; a creature that hunts in the dark gets `darkHunter: true`; code that
should know about Rest binds `UWRSurvivalComponent::OnRest`.

Also since 0.7.1: **farming and food** (FULL-GAME 6.2, cooks/build-farming.md): `UWRFarmSubsystem` runs plots
(growth 0..1 moved every 2 s from `FWRFarm::GrowthRate`: land, plot type, light / dark / wind, water, bees, companions,
Ashfall), the water network (wells, channels fed end to end), pens, hives, traps, salt pans, ice boxes and the freshness
sweep; `AWRBuildPiece` carries the farm state (`CropId`, `Growth8`, `FarmFlags`, `FarmStop`, `Feed`) and `FWRFarmLook`
draws it; `UWRFoodComponent` (PlayerState) holds known / hinted dishes, Sated, first harvests and legends. Crops, plot
types, animals and wild seeds are `Data/farming.json`; food groups, dishes and fish recipes `Data/cooking.json` (dishes
become `dish_<id>` station recipes the Cook screen orders, hidden from the Crafting tab). Food stacks carry freshness
(`FWRItemStack::Fresh / FreshAt / FreshRate`, `FWRSpoil`): move stacks with `FWRInventoryRules::MoveFrom` / `AddFood` so
freshness and worth travel with them. New rules for new code: a new crop is a `farming.json crops` row (seed and produce
items, an icon); a new farm animal an `animals` row; a dish a `cooking.json dishes` row plus its item; a land's farm
pieces, crops, fish, lures and recipes carry `biome` and stay hidden until it's in `tuning world.liveBiomes`.

Also since 0.7.1: **crafting to tier 5** (`WRGear.cpp`, `Data/gear.json`, cooks/build-crafting.md): steel and
starsteel, a charcoal kiln, star iron in the highland caves, core shards; the armour slot is now **Head, Chest
(`EWREquipSlot::Armor`), Hands, Legs** plus **Talisman** and **Ring / Ring2** (`WRGearSlots::All`, `GetEquipped`); the
12 biome sets and their Ascendant pieces with 2 / 4-piece bonuses; rings, talismans from trophies, runes, salvage,
removing extras, blueprints (`AWRPlayerState::KnownBlueprints`, saved). Rules for new code: anything that reads worn
gear asks `UWRInventoryComponent::GearStat(<stat>)` (or `GearStatFor(actor, ...)`): it adds the weapon's sockets and
innate effects, the active set bonuses, the talisman and the rings. A stat named like a life-skill stat already adds to
`StatFor`. A land's lane wires its set's and talismans' stats (fallDamage, glideSeconds, passWards, spellCost,
riftDrain, breathSeconds, sprintStamina...: the list is in gear.json and items.json `worn`). A recipe whose effect has no
system yet gets `"waitsFor"` and stays hidden. Saves carry `gearSlots: 2`; an older one migrates the armour.

Also since 0.7.1: **machinery and power** (`WRMachine.cpp`, `WRMachineLook.cpp`, `WRUIMachine.cpp`, `Data/machines.json`,
cooks/build-machinery.md): `UWRMachineSubsystem` (server) joins machine pieces whose ports meet into networks (rebuilt
when a piece comes or goes), runs them in 0.5 s steps near players, and writes each piece's `FWRMachineRep` (speed,
state, line totals, progress, battery, a lift's trip) which every machine draws from; `UWRMachineLookSubsystem` turns the
modelled parts from that (one drive angle per machine, eased), moves belt items from the belt's run and moves the lift
platform's collision (the server too). A machine's ports are defined once, in `FWRMachineLook::LocalPorts`, next to its
model. Machines use the Hearth level of a Hearth their builder owns anywhere (not the covering one), so a wheel on a
beach or a drill in a cave can be built. A station a shaft runs into is retimed through `AWRStation::AppliedBoost`
(queue `ItemLeft` / `ItemSeconds` are at 1x). Planks pay for wood in build costs (`FWRMachines::CanPay / Pay`).

Also since 0.7.1: **building past wood and stone** (cooks/build-building.md): `Data/building_sets.json` holds the tiers
(metal, the 12 biome sets, wood and stone for beams), the 12 shapes' cost / HP shares, the freeform pieces and each set's
decor; `FWRBuildSets::Expand` (`WRBuildSets.cpp`) turns them into ordinary `FWRPieceDef` rows inside
`FWRBuildRegistry` (ids `<tier>_<shape>`, `<tier>_beam2/_beam4/_post/_brace`, `<tier>_<key>`), so placing, saving,
upkeep and the build bar treat them like wood and stone. Their look is `WRBuildLookSets.cpp` (one style per tier over the
unchanged collision) and `WRBuildLookDecor.cpp` (furniture archetypes, also the comfort decor in pieces.json with
`arch`), drawn with one material, `M_BuildTinted` (`Tools/make_build_materials_tinted.py`: colour, metal, roughness,
glow and pattern per instance). Support is `FWRIntegrity` (`WRBuildIntegrity.cpp`): `LinkCost` between any two
structural pieces (Raven's grid rule unchanged, beams by their ends and length, a brace's +1), `Allowed`, the build-mode
overlay colours (`M_BuildOverlay`, `AWRPlayerController::UpdateBuildTint`) and `AWRRubble` for a collapse. Beams carry
their slope in the place command (`p<deg>`) and the save (`pitch`). Turrets are machines (role `turret`, `WRTurret.cpp`
server brain, model and aim line in `WRMachineLook.cpp`, ammo rows in `machines.json` `turrets`). Rules: no base piece in
a cave (`FWRBuildSets::PlaceCheck`), weapon / tool hits on pieces scaled by `materialDamage`.

Since cookpass-building (cooks/cookpass-building.md): **the build mode on a pad** is RT place, LB / RB turn (held:
repeats, tuning `building.padTurnStep / padRepeat`), D-pad left / right the piece, up the category, X copy (gamepad.json
verbs `BuildPrev / BuildNext / BuildCategory / BuildCopy`, read in `WRPadInput.cpp` while placing; the attack buttons in
`AWRCharacter` placing branch). **Copy** (`AWRPlayerController::CopyAimedPiece`, key `BuildCopy`, middle mouse). The bar's
tab lists live in `WRUIBuild.cpp` behind `WRBuildBar.h` (`Step`, `NextTab`, `OpeningTab`: Survival = pieces.json
`"starter": true` until you own a claim, then the last tab, `GameUserSettings [Wildreach.Build] LastTab`); the chip's
short words are `ShortWhy` (the long reason under it). **Building from storage**: `WRBuildStock` (chests you may open,
inside the claim you stand in, within `building.storageReachCm`) counts on the bar and the ghost and pays on the server
(the bag first). The **lean-to** (`lean_to`, kind `shelter`, arch `lean_to`) is a roof for `WRBiome::IsUnderBuiltRoof`.

Also since 0.7.1: **taming past the tame** (`WRTaming.cpp`, `WRTameLook.cpp`, `WRSealBody.cpp`, `WRUITameCard.cpp`,
`WRUIIncubator.cpp`, `Data/tame_trees.json`, tuning `tame`, cooks/build-taming.md): every creature carries genes
(`FWRTameGenes`: points in six stats, mutations per line, morphs, sex) rolled at spawn by its band, and every machine
reads `AWRCreature::TameRep` (levels, bond, tree, saddle, barding, growth, care, breeding, hunger, fly stamina, the
stat multipliers). `AWRCreature::RecomputeStats` is the one place a creature's health, bite, stamina, torpor and speed
come from (genes x bond x tree x growth x taming effectiveness); call it after changing any of those. Tame XP is
`GainBondXP(amount, why)`; role-tree effects ask `TameHasNode(<id>)` (ids in tame_trees.json). The combat hooks are
`FWRTaming::AttackerScale / PlayerTakes / StanceTaken / KeepsSeat / AfterHit` inside `ApplyHit`, the tack's share in
`AWRCreature::ModifyIncomingDamage`. Mount kinds are creatures.json `mountKind` (ground / pack / swim / fly; haulers
default to pack): `ApplyRideSpeeds` and `TickMount` run on the server and the rider's machine. Saddles, pack saddles and
barding are items with `tack`; they're modelled in code from their parts (`RidingSaddle`, `PackSaddle`, `BardingBody`)
on a back fitted per species by creatures.json `seat` / `back`, attached to `spine2` (neck lames and face plate to
`neck` / `head`). Mutations show through `M_TameMorph` (`Tools/art/make_tame_morph_material.py`) as the mesh's overlay.
The Floeback's body is built and moved in code (`UWRSealBody`). Card and incubator commands carry the tame's /
incubator's position (`tamecard x y z <verb>`, `incubator x y z <verb>`) so the server acts on the one the screen shows.
Dev: `WRTameDev now | bond N | grow X | care | mate | hatch`.
The taming cook pass (cooks/cookpass-taming.md) added: a tame's **name** (`TameRep.Name`; `AWRCreature::TameName()`,
`FWRTaming::Label()` for lines to its owner; card Rename with suggestions from `Data/tame_names.json`), **Release asks
twice** (`ReleaseArmedBy`), **every land's element** on a tame's plain blows (`FWRTaming::ElementHit`, tuning
`tame.element.<element>`), **ride keys from the bindings**, the **rider's seat** (a procedural seated pose in
`FWRHeroAnimProxy::PostLayers`, numbers in tuning `tame.seat`; the body follows the saddle bone's bob and is eased
onto / off the seat in `AWRCharacter`'s anim update, `tame.mountEase`), **getting off onto clear ground**
(`AWRCreature::DismountSpot`), **Call your mount** (the whistle wheel's wedge, `FWRTaming::CallMount`, `CallTo` on the
creature), the **courting / cuddle nuzzle** (a head dip through the pose lift), the **stable** (piece kind `stable`,
`WRUIStable.cpp`, `FWRTaming::StableCommand`, its tames as save lines in `AWRBuildPiece::StableJson` with a replicated
`Roster`, 3 a claim checked in `WRPlayerBuild`), the **bond token** and **Dadeni draught** (a dead tame's line kept in
`UWRTamingSubsystem::Fallen`, saved as the world save's `fallenTames`; the token's sockets carry its id; use it at your
Hearth) and **Soulbond** (`FWRTaming::SoulbondIncoming` inside `ApplyHit`; `TameRep.bSoulbound / bSoulDown`). The card
has tabs (Overview, Skills, Gear, Family, Packs; LB / RB on a pad) with real stat values and its parents.

The creature cook pass (cooks/cookpass-creatures.md in showcase-notes) added **body language** over every skinned
creature's and boss's clips: `WRCreatureBody.cpp` fills `UWRAnimInstance::Input.Body` (head aim spread over the neck
chain, dips, the spine's flinch / pant / slump / gestures) and `FWRAnimProxy::ApplyCreatureBody` draws it after the pose
lifts, before the feet. The chains are found by bone name (`SetupCreatureBody`: the head and its "neck" parents, then
"spine" / "chest"). Every landed blow kicks `KickFlinch` from `HitOriginShown` / `HitDamageShown` (set in `MulticastHit`).
Numbers in tuning `anim.look / flinch / pant / wounded / slump / hungry / beat`. A creature that should look somewhere
new gets a case in `AWRCreature::BodyLookAt`, not a new system.
Moods are `EWRCreatureMood` in `AWRCreature::MoodRep` (+ `WatchRep`), set only by the server's brain through
`SetMood` (it falls back to calm after `creatureAI.mood.staleSeconds` without a refresh, so a biome hook that takes the
brain's turn never leaves a stale pose). A biome's own creature that wants a pose sets a mood the same way.
Attack rows shape the body by their `motion` (tuning `anim.gesture.<motion>`, sent by `MulticastRowGesture` from
`StartRow`): a new motion needs a tuning row, not code. Creature calls `alert` / `flee` / `answer` fall back through
sounds.json `voiceFallback`; a species' own recording is just a `creature.<id>.<kind>` row. A creature on a shared mesh
gets its own silhouette with creatures.json `shape` (head / neck / tail / wings), never a bone edit in code.

Also since 0.7.1: **world systems** (`WRWorldSystems.*`, `WRWorldSites.cpp`, `WRWorldNpc.cpp`, `WRUIWorld.cpp`,
`Data/world.json`, cooks/build-world-systems.md): `UWRWorldSystems` (server world subsystem) places the villages, shrines,
path masters, summoning stones and hidden tubes once their ground traces to land (every data position is settled at
runtime, stepped inland if it lands in the sea), runs Starfall, the biome events (they force their `weather.json` row:
`eruption`, `whiteout`), invasions (invaders are raiders: `bRaider`, `RaidBase` walked from the far pass mouth to the
target) and the caravans (placed on their loop by the game clock, actors only near players), and keeps
`world_systems.json` in the world folder (shrines, who found them, messages, schedules, caches). `AWRWorldState`
replicates the events and the invasion; each player's quests, found shrines, kindled shrines and standing are
`UWRQuestLog` on the PlayerState (owner-only, saved as `world` in the player file). Masters and merchants are
`AWRWorldNpc`, an `AWRCharacter` with no PlayerState (hero body, `bNpcMarks` for their path marks); masters have no
health, merchants can only be hurt by creatures (`WRWorld::BlocksHit`, called from `ApplyHit`, also seals a summoned
boss to its party and notes invasion damage). Rules for new code: a new biome is rows in world.json (biomes, shrines,
masters + quests, villages, caravans + trade, events, invasion pairs, hidden, summoning effigies), a `night` table in
spawn_regions.json and its event weather row; quest kinds are `bring / show / kill (+status) / shrines / quench / sneak /
tame` (`UWRWorldSystems::QuestEvent`); world screens open through `ClientOpenWorldScreen` under `EWRMenu::World`;
"world ..." is the one server command family. Dev: `WRCmd world dev ...`.

Also since 0.7.1: **dungeon portals and artifacts** (`WRPortals.*`, `WRPortalFold.cpp`, `WRPortalThings.cpp`,
`WRPortalBosses.cpp`, `WRArtifacts.*`, `WRUIPortals.cpp`; data `Data/portals.json`, `Data/portal_rooms/`,
`Data/artifacts.json`): ranked portals that break on real days and spill, handmade rooms shuffled into folds with traps,
trials and a boss with an arena mechanic, keys, personal chests and shards; the artifact belt (journal tab N), the band,
Artifact Level, geis records, fusing. Saved in the world's `portals.json` and each player's file (`artifacts`).

Also since 0.7.1: **the endgame** (`WRDeeds.*` + `WRDeedsAchieve.cpp`, `WRRecords.*`, `WRWarHall.*`,
`WREndgameLook.*`, `WRUIEndgame.*`; data `Data/achievements.json`, `souls.json`, `cosmetics.json`, `mythic.json`,
`raids.json`, tuning `prestige` / `deeds` / `records` / `monuments`): prestige at the boss altar (its screen has Path,
Return and Forge soul pages), the Heron's draught (respec item + The Unlearning quest lines), boss soul forging and Soul
arts, mythic rules (data-ready, hidden), kill-time boards with stone tablets, 150 achievements / titles / cosmetics in
the journal's Deeds tab (key `.`), boss monuments, and the Thaw Hall raid (gate by the North pass road; hall in hall
space at X -1,500,000). Each player's deeds save in their character (`deeds`); boards in the world document `records`,
war-hall rests in `warhalls`. Testing aids on Development builds: `deeds dev give <achievement>`, `deeds dev soul
<boss>`, `deeds dev cosmetic <id>`.

Open, as of 0.7.1:
- **The endgame on two lands**: souls, monuments, glows and achievements show only for bosses and lands in this build
  (`live` / `needs` in their data); the other five war-halls and mythic forging wait for their lands and the Ring on the
  Crown. The war-hall gate stands by the North pass road until the pass forts exist. Raid champions reuse Vulkarn's and
  Skaldrith's rigs with overlays.
- **Farming on the island is two lands**: 4 crops, 2 farm animals, the volcanic and tundra fish; the other 10 lands'
  crops, fish, lures, animals, paddies, kelp lines, log beds and the beehive (Hiveback) are in data and hidden until
  their land is listed in `world.liveBiomes`. Greenhouse glass needs Hearth 4 (steel). Hungry animals show it only on
  the prompt; no chimney smoke yet; bone meal waits for the grinder.
- **Crafting tiers 4 and 5 need 4 and 6 major bosses**; the island has Vulkarn and Skaldrith, so steel, the biome
  sets, rings above iron and the boss-weapon blueprints' recipes show "Needs tier 4/5" (`WRHearthLevel` for tests).
  Boss weapons use their class's moves; set-decor blueprints and the Old Tusker talisman wait for their
  systems.
- **Survival on the island is two biomes**: only Ashfall / Blizzard and the lesser volcanic and tundra weathers can
  happen; corruption, dark nights, strikes and gusts are live in code and data for the biomes that don't exist yet. The
  hero has no sleep clip (the death clip's end is held on the bed; `rest.pose*` in tuning).
- **No packaged dedicated server** (on hold, Raven's call). The host's PC is the server. Unreal built from source sits
  at `C:\Dev\UE5Source` on Raven's PC (`Tools\build_engine.ps1`), needed only for that.
- **The gate playtest** (a group of friends plays a full evening and wants to come back) hasn't been run.
- **Volcanic shrubs stay green**: the ash-grey coat hung the GPU (VSM Nanite), so `ashCoatMin` stays 0.
- **Building tiers 4 and 5 and the set decor** need a Hearth of 4 / 5 and the land's major boss: on the island metal
  waits for a fourth core, the Basalt hall and Rime lodge for a fifth, and their decor blueprints drop from Vulkarn and
  Skaldrith; the other ten sets (structure, beams, decor) stay hidden until their land is live. The hot spring bath
  goes by lava or inside a hot spring (island_places.json kinds in `building.hotSpringKinds`: the tundra's Steam Ground). The other ten paths' element-turret essences wait
  for their items (rows in `machines.json`).
- **Taming on the island is two lands**: the Windmane (first flying mount) is in creatures.json but never spawns (its Sky
  islands aren't on the map) and wears the Griffon pack body only for testing flight (`WRSpawn windmane`, `WRTameDev
  now`); it needs its own four-winged sky-stag body. Every live land's barding pass is wired (cookpass-taming: `FWRTaming::BardingPass /
  BardingStat`); the desert's Dunewrap waits for its land. The Floeback's body is modelled in code, not
  sculpted art. Saddle seats per species (`seat`, `back`) are first fits to eyeball. Pen-born farm young still grow on
  the farm's clock, not the baby system. The stable, names, the Dadeni revive and Soulbond are built (cookpass-taming);
  not built: the bone altar's permanent undead tames (necro raising by ritual), each species' own whistle-answer call
  (they answer with their idle call), and the rider's seat is a procedural pose over the idle (no riding clips yet).
- **World systems on the island are two lands**: two villages, ten shrines, two masters, two caravans, the Kiln Roads and the
  Blue Below; masters speak in text, merchants have no pack beasts, the tubes are boulder corridors whose routes nobody has
  walked yet (tune `world.json hidden.route`), and the eruption river rise, event music, invader ice / slush, the Dimming
  and the Migration are not built (cooks/build-world-systems.md 4).
- **Portals and artifacts on the island are two lands** (cooks/build-portals-artifacts.md 4): portals open in the Volcanic
  and the Tundra only (`Data/portals.json biomes.<land>.land`; a land's kit is `Data/portal_rooms/<land>.json`), their
  folds hang in bays high over the sea east of the island (`portals.json bay`; anything there with no fold round it is
  put back at its portal); rank 3-5 bosses borrow built rigs (overlay colours in creatures.json) until they're sculpted;
  the new creatures' voices are pitched shared calls; only four artifacts can drop until more lands, majors and raids
  exist. Dev: `WRCmd portal dev open <land> <rank> | crack | spill | break | clear`, `WRCmd art dev give <id> <weight>`.
Also since 0.7.1: **Storm peaks, the first land off the Old Island** (cooks/biome-lightning.md, build plan
cooks/build-biome-lightning.md in showcase-notes). How a new land is put together, for every land after it:
- **The ground** is a second landscape in `L_Island` (`Landscape_StormPeaks`, 4,032 m square at 2 m, north of the island,
  overlapping its top 112 m so the north-west neck joins the island's shore). It comes from `Tools/terrain/storm_peaks.mjs`
  (heights from the cook's landmark heights, the Crown, the Ring, gorges, needles and the Rodwardens' graded paths carved
  last) and `Tools/terrain/build_storm.ps1` (heightmap, material `M_StormAuto` from `build_terrain_materials.py`
  `WR_MATERIALS=storm`, then `import_storm.py` in the unattended editor, which also places the three Storm camps'
  PlayerStarts tagged `storm_peaks`, their safe zones and starter stations). Re-running replaces the old one.
  `Art/Terrain/storm_height.r16` isn't committed (the script makes it). After a terrain change also run
  `node Tools/terrain/storm_walk.mjs`, `node Tools/terrain/sea_mask.mjs` and `python -I Tools/terrain/world_map.py`.
- **Which land a point is in**: `WRBiome::At` answers `storm_peaks` for anything north of `world.islandNorthEdge`
  (the island's frame). Its sub-zones (1-6, `WRStorm::ZoneAt`) are a 32 m grid in `Data/storm_peaks.json`.
  `world.liveBiomes` lists the lands in the game; a land's crops, items, recipes, sets, machines, deeds, shrines,
  masters and events show only once it's there.
- **Everything on it is built at runtime from `Data/storm_peaks.json`** (generated, edit the script): `UWRStormWorld`
  (`WRStormWorld.cpp`) builds the landmarks, hand sites and kit POIs as code-modelled scenery on every machine
  (`AWRStormScenery`, one look per kind), the rods (`AWRStormRods`), and on the server the rope bridges with their planks
  (`AWRStormBridge`), site caches, caves (`AWRCave` with `Theme`), ore and fulgurite nodes, ball lightning. The island's
  dressing file also holds Storm peaks' trees, boulders, scree and shrubs (`storm_pine`, `granite_rock`, `slate_scree`,
  `storm_shrub`; `dressing.mjs` keeps them when it rewrites the island's).
- **Lightning strikes** from any source go through `WRStorm::CallStrike` (`WRStormTypes.h`): the 3 s body pre-tell (hair
  lift through `AWRCharacter::StrikePreTell`, corona, buzz), the 1 s ground glow with its stepped leader, the stroke,
  thunder delayed by distance, rods taking it within 3 m, ground current on wet ground, fulgurite. Creatures listen to
  `WRStorm::OnStrikeLanded()`. The weather's picker (`UWRWeatherSubsystem::TickStrikes`) weighs heavy armour / Stormweave,
  skips Grounded and charm wearers when it can, lets a Rodcrest draw the bolt.
- Wildlife regions take any land as `side`, with `zones` (sub-zone letters) and a `storm` table; the walk map
  (`island_walk.json`) is now a 249-cell square that covers both lands (`storm_walk.mjs` grows it from `walkmap.mjs`'s).
  The map picture covers both (`ui.mapMin / mapSize / mapPixels`); the sky and the ambient beds blend into Storm peaks'
  north of the island; music looks for `<pool>.<land>`, `boss.<id>`, `mini.<land>`, `sub.monastery`, `camp.<land>`,
  `pass.stormgut` before the shared pools.
- Shrines and masters can stand without a village (`at` / `yaw` in `world.json`); quests have a generic `event` type
  counted by the land's own system (`rodroad`, `pretell`, `lantern`). Drinks can give a timed buff (`use.buff`:
  `grounding`, `shock_ward`; `WRStorm::HasBuff`).
- **Creatures and bosses** of Storm peaks: `WRStormCreatures.*` (a `UWRStormBody` on each: its code-built silhouette
  parts on bones, charge glow, the land's specials and tame uses; `UWRStormLife` for the living-world beats) and
  `WRStormBosses.*` (`FWRStormBoss` hooks for the Bellwether, Galvara, Coilback and Kharvox; `UWRStormArenas` places the
  arena spawners, rim rods, altar and run-back stones once per world). Rows in `creatures.json` / `attacks.json` /
  `spawn_regions.json`; numbers in tuning `creatureAI.storm*`, `bosses.<id>`, `tame.storm`. The bodies are the closest
  existing rigs retinted (`look.tint`) with code-built parts: own sculpts need the art PC.
- **The Lightning path**: `Data/paths/lightning.json`, `WRLightningPath.*` (Charge, Grounded, passives, procs),
  `WRAbilitiesLightning.*` (every ability node), `WRCombos.*` + `Data/combos.json` (path pairs from data, Fire+Frost
  included), `WRPathLook.*` (a path's colour, icon and resource name from its file: nothing in the UI names fire/frost
  any more). The main menu's path picker lists every path file.

Also since 0.7.1: **the Swamp, the second land off the Old Island** (cooks/biome-swamp.md, build plan
cooks/build-biome-swamp.md). Built exactly like Storm peaks (above): `Landscape_Swamp` from `Tools/terrain/swamp.mjs` +
`build_swamp.ps1` (`swamp_height.r16` not committed); everything on it from `Data/swamp.json` (generated by swamp.mjs:
edit the script, then re-add the World-track hand sites `cage_yard`, `house_yard`, `mere_shore`, `cathedral_floor`
which are in both). `UWRSwampWorld` (`WRSwampWorld.cpp`) builds scenery and spawns boardwalks, nodes, caches, caves;
`UWRSwampSystems` (`WRSwampSystems.cpp`) runs bleedwater/quag/gas/leech boils, the Red Tide (state on `AWRSwampState`),
the Sunken Coven (300,000 cm above the swamp, `swampHazards.coven.lift`), secrets, Ruslana; `WRSwamp::Prompt/Command`
route the swamp's interacts (`world swamp ...`). Creatures `WRSwampCreatures.*` (`FWRSwampAI`, `UWRSwampBody`), bosses
`WRSwampBosses.*` + `WRSwampVharra.cpp` (`FWRSwampBoss`, `UWRSwampArenas`). The Blood path is `WRBloodPath.*`.
Numbers: tuning `swampWorld`, `swampHazards`, `redTide`, `swampCreatures`, `swampBosses`, `swampVharra`, `blood`.
Dev: `world swamp dev coven | dig | candle`, `swampboss vharra ...`.

Also since 0.7.1 (in progress): **the Corrupted wasteland, a runtime land** (cooks/biome-void.md, plan
cooks/build-biome-void.md). Unlike Storm peaks and the Swamp it is not a landscape: `WRLand.*` builds its ground from
`Data/lands/wasteland.json` (a pure height function + data features, procedural-mesh chunks on every machine), 9 km
south of the island (move it with `origin` / `mapCentre`). Places: `WRLandSites.cpp`; boats, lift and other live
things: `WRLandThings.*` (`land ...` commands); rifts and MaxHealthLost: `WRVoid.*` (tuning `void`). A new land is a
new file in `Data/lands/` plus its biome in `world.liveBiomes`. Its gathering, keepsakes and thrown things:
`WRVoidWorld.*` (wasteland.json `resources`, tuning `voidWorld`); its creatures and dens: `WRVoidCreatures.*` (the
Swamp's hook pattern, wasteland.json `spawns`, tuning `voidCreatures`). Shrine and master rows with `"land"` are placed
on that land's height function (`WRLand::PlaceOn`). The Void path: rules `WRVoidPath.*` (tuning `voidPath`, spells.<void
spell>, nodeEffects.void.*), abilities `WRAbilitiesVoid.*` (pay through `WRVoidPath::Pay`; registry in WRAbilities.cpp). Its bosses:
`WRVoidBosses.*` (FWRVoidBoss hooks beside the Storm / Swamp ones in WRBoss*.cpp; arenas from wasteland.json `bosses`;
the Null Gate and Vorhollow's stage are actors whose motion is a function of their replicated clocks). Its hazards and
the Rift Storm: `WRVoidHazards.*` (one replicated `AWRVoidState` per land; the event schedule is world.json's). The
Unmade Place and the Descent lift: `WRVoidUnmade.*` (E prompts through `WRLandThings`: `land lift`, `land winch`,
`land keep`). In these files keep file-scope helper names prefixed (unity builds join files).

The Ancient forest is drawn with Epic's Electric Dreams scans where `Data/forest_art.json` has a kind (answers 11-16):
oak/beech/lime/ash are the eight hornbeam scans (leaf colour per "species"), saplings the hornbeam saplings, ferns,
mossy boulders; plus a ground layer (`Data/forest_ground.json`: moss at every foot, leaf litter in the hollows, wood
anemone and elder in the light), all from `WRForestArt` in `AWRDressing::Build` / `BuildForestGround`. Collision is the
data's capsules (measured off the scans by `Tools/art/electric_dreams_forest.py`), so servers and PCs without the kit
agree; without the kit a kind draws its tree-kit look. The kit is git-ignored like the Fab packs (`Content/Megascans`,
`MSPresets`, `Custom`, `PhysicalMaterials`; list in `Tools/art/electric_dreams_forest.json`). Re-run
`node Tools/terrain/forest_dressing.mjs` after `forest_art.json` changes (the moss rings hug the boulder each instance draws);
its ground layer has its own random stream, so the trees (and saved harvests) never move.
Also since 0.7.1 (in progress): **the Ancient forest, a heightfield land** (cooks/biome-forest.md, plan
cooks/build-biome-forest.md). Its ground is `Data/lands/forest.json` + `forest_ground.bin` from
`node Tools/terrain/forest.mjs` (also writes `Data/forest.json`, the places contract read by `FWRForestData`), then
`forest_walk.mjs` (after `storm_walk.mjs`), `forest_dressing.mjs` (trees into `island_dressing.json`), `sea_mask.mjs`
and `world_map.py`. Code: `WRForest*` (world, scenery, hazards, Hollow Root, creatures, bosses, arenas),
`WRTreeKit`, `WRNaturePath`/`WRAbilitiesNature`/`WRPlantSummon`. Numbers: tuning `forest`, `forestWorld`, `hazards`,
`creatureAI._forest`, `tame.forest`, `bosses.forest` and `bosses.<id>`. Dev: `world fhaz ...`, `world forest ...`.

Also since 0.7.1 (in progress): **the Graveyard, a heightfield land west of the Spine** (cooks/biome-graveyard.md,
plan cooks/build-biome-graveyard.md). Its ground and places are one generator: `node Tools/terrain/graveyard.mjs`
writes `Data/lands/graveyard.json` + `graveyard_ground.bin` and `Data/graveyard.json` (read by `FWRGraveData`); it reads
the forest's `forest_ground.bin` (the Spine's west face and the Rootgrave seam) and `Art/Terrain/island_height.r16`, so
re-run it after a forest re-run. Lands overlap now: a land's `priority` and `cuts` rows (WRLand) decide whose ground
is where. Then `node Tools/terrain/graveyard_walk.mjs` (after `forest_walk.mjs`) grows the wildlife walk map over it.
Code: `WRGrave*` (types, world, scenery, catacombs, creatures). Numbers: tuning `graveyardWorld`, `graveyardLife`.
Dev: `world grave ...`. The Risen are skeletons built on the Mixamo rigs' bones (look.hideMesh): their clips drive them.
Hazards (subsidence lids, lime, draughts) are `UWRGraveHazards` (server, numbers in tuning `hazards.graveyard`); the
sky's Graveyard air and its moonless nights are in `WRSky` (`graveyardWorld.sky`); the Blood Moon sets `WRGraveSky::BloodMoon`.
The bosses are `WRGraveBosses.cpp` (the fights: hooks AWRBoss calls beside the forest's, `friend struct FWRGraveBoss`),
`WRGraveBossLook.cpp` (`AWRGraveBossProp`: the Belfry's bell machine, egg sacs, the chandelier, chapel tombs, dug graves,
thrown headstones, the Procession; `UWRGraveBossBody`: their code-built bodies) and `WRGraveArenas.cpp` (placement when
a survivor is within `bosses.graveyard.placeRadius`; E actions `world grave boss help|pull|stake`). Arena layouts come
from `Data/graveyard.json` (sites L1/L3 and rooms C5/C9 carry `arena`; the Gravedigger's lair, route and stops) and
`WRGraveChapel` / `WRGraveBelfry` in `WRGraveTypes.h` (the scenery and the boss share them). Graveyard sound rows are
stand-ins (`_standin: true` in sounds.json): replace their files when real recordings exist.
The Necromancy path is `WRNecroPath.*` (Souls attributes, Deathless, the minion and body tables, the passives the combat
pipeline calls: `DamageScale`, `Incoming`, `ScaleBuildups`, `AfterHit`, `OnKill`; `CreatureDied` from `AWRCreature`'s
death) and `WRAbilitiesNecro.*` (ClassForNode "necromancy"). A raised minion is a fresh creature with `TamedBy` set and
the `Status.Necro.Raised` tag: `CountTames`, the world save and the death loot skip it. Not built yet: the bone altar's
permanent undead tames, Jolt of life / Blood bond / Rot and bloom, the Necromancy node icons (the path screen shows
the nodes without their glyphs).
The Blood Moon is `UWRGraveEvents` (world.json events.bloodmoon is scheduled by the world systems like the Bloom;
`UWRGraveEvents::Sign / Rising / On` are what the rest asks). The Graveyard's ambient beds are `land.graveyard[.night]`
and `zone.G1..G4 / zone.neck` rows; its music pools `<pool>.graveyard` and `boss.nhal`.

Also since 0.7.1 (in progress; not live until Desert (3)): **the Desert / Sun & sand** (cooks/biome-desert.md, plan
cooks/build-biome-desert.md). `node Tools/terrain/desert.mjs` writes `Data/lands/desert.json` + `desert_ground.bin`
(priority 2: `cuts` take the Fungal strip's beach south of y -7,800 and the Ocean's west sea west of x -280; it reads
`fungal_ground.bin`, `ocean_ground.bin` and `Art/Terrain/storm_height.r16` for its seams, so re-run it after any of those
change) and `Data/desert.json` (read by `FWRDesertData`, `WRDesertTypes.cpp`). The cook's coordinates go through the
script's `X()` / `Y()`: keep writing cook numbers there. **The surface class under a point is the desert's rules**
(`WRDesert::SurfaceAt`, `EWRDesertSurf`): soft sand, slipfaces, quicksand, brine, wet sand, hardpan... the movement, the
hazards and the sand-swimmers all read it. Scenery: `WRDesertScenery.cpp` (landmarks, shrines, camps, caves, the
Hourhold) / `WRDesertSites.cpp` (sites, kit) on the forest's FBuild with the kit `WRDesertLook.h` (`Ds*`); node looks
`WRDesertNodes.cpp` (asked first in `WRResourceLook`); `UWRDesertWorld` builds and runs it (`world desert spot`);
`AWRDesertState` replicates the Lee Door, the Dig, the Mirror Gallery and the Sun-Vault. Numbers: tuning `desertWorld`.
The Sun & sand path is `WRSunPath` (Radiance, Sandwalker = `Passive.Sandwalker`, the passives the combat code asks
about: `ScaleBuildups`, `DamageScale`, `AfterHit`, `OnParry`; `WRSun::Exposed` is the one "under the open sky" test,
`WRSun::SandUnder` the one "sand underfoot" test for any land) and `WRAbilitiesSun` (its abilities, `AWRSunField` the
beacon / storms / quicksand, `AWRSunColumn` the light columns). Numbers: tuning `sun.radiance`, `sun.path`,
`spells.<key>`, `wildbrands.<node>`, `nodeEffects sun.*`.
The Desert's creatures are `WRDesertCreatures` (`FWRDesertAI` hooks friend of `AWRCreatureAI`, like `FWROceanAI`; `UWRDesertBody` holds each one's server state and multicasts its moments; `UWRDesertLife` keeps the populations by sub-zone from tuning `desertLife.tables`) and `WRDesertBeast.cpp` (the code-built bodies). Numbers: tuning `desertLife.*`; attacks in `attacks.json`.
The Desert's hazards and the Long Noon are `WRDesertHazards` (`UWRDesertHazards`; the slows are replicated tags `Desert.*` that `AWRCharacterBase::GetSpeedMultiplier` reads through `WRDesertHz::MoveScale`; sinking bodies are `AWRDesertState::Sinkers`, eased into the body offset on every machine; floods, brine and the Long Noon's stage are on `AWRDesertState` too; `WRDesertHz::SkyHour` holds the sky at noon). Numbers: tuning `hazards.desert.*`, `desertEvents.longNoon.*`.
The Desert's bosses are `WRDesertBosses` (`FWRDesertBoss` hooks, friend of `AWRBoss`; `AWRDesertStela` the plaza's breakable cover; `UWRDesertArenas` places them; `AWRDesertState::bGnomonUprooted` hides the plaza's gnomon while Ahrumet carries it; Ahrumet's body is the `colossus` kind in `WRDesertBeast.cpp`). Numbers: tuning `bosses.<id>`; moves in `attacks.json` bosses.
The Desert's music and beds are picked in `WRAudioDirector` (the Desert branch beside the Ocean's; `BeltBlend` low-passes the music in the belt; `LongNoonLayer`). Rows: `Data/Audio/sounds.json` (`desert.*`, `zone.D*`, pools `*.desert`, `boss.ahrumet.p1..p3`).

Also since 0.7.1 (live from Ocean (4)): **the Ocean & reefs** (cooks/biome-ocean.md, plan
cooks/build-biome-ocean.md), the strait between the Sky's Chain Cliffs and the Storm crown. `node Tools/terrain/ocean.mjs`
writes `Data/lands/ocean.json` + `ocean_ground.bin` (heightfield, sea floor drawn) and `Data/ocean.json` (landmarks,
sites, kit, decor, caves, the Drowned Hall, nodes, spots, caches, hazards, arenas; read by `FWROceanData`,
`WROceanTypes.cpp`). The cook's coordinates are world-map's: the script's `Y()` squeezes them into this world, so keep
writing cook numbers there. Scenery: `WROceanScenery.cpp` / `WROceanSites.cpp` on the forest's FBuild, kit
`WROceanLook.h` (helpers `Oc*`), node looks `WROceanNodes.cpp`; `UWROceanWorld` builds and runs it (`world ocean spot`).
**Underwater play is game-wide** (`WRUnderwater.*`): "under" = the head is inside the highest-priority physics volume
there and it's water, so any new water (a flood, a cistern) only has to be a water volume (an air pocket is a non-water
volume of higher priority); breath and depth are `UWRDiveComponent`; Dive / Rise are the saved move's FLAG_Reserved_1 /
_2 (all four custom bits were taken). **The tide** (`WRTide.*`) is a pure function of the game clock plus
`AWROceanState`'s King Tide; anything that asks "where is the water's surface here" calls `WRTide::SurfaceAt` (the still
sea elsewhere).
The Tide path is `WRTidePath` (Flow, Gills = `Passive.Gills`, passives, combos, look; named so because `WRTide` is the
sea's tide) and `WRAbilitiesTide` (its abilities, `AWRHarpoonLine` the shared harpoon line any thrown harpoon can use:
`Make`, `TieTo` a mooring stone, `AddPull`, `On(body)`; `AWRTideWave` the travelling waves, `AWRTideField` the whirlpools).
The sea's creatures are `WROceanCreatures.*` (`UWROceanBody` state + `FWROceanAI` hooks, friend of `AWRCreatureAI`,
called beside the Sky's; `UWROceanLife` populations from tuning `oceanLife`) and `WROceanBeast.cpp` (the code-built
bodies, kinds by creatures.json `procBody`; Old Barnacle, the Siren Eel and the Reef Tyrant reuse crab / eel / shark at
their scale). A sea creature holds its depth through `UWRCharacterMovementComponent::bHoldDepth` (neutral buoyancy);
the Wavecharger's dive is `diveDepth` in `AWRCreature::TickMount`.
The hazards and the King Tide are `WROceanHazards.*` (`UWROceanHazards`, every machine: rips are a pure function in
`CurrentAt` read by the movement component; blowholes, surges, the man o' war touch and the King Tide run on the
server; the fleets are drawn locally from `ManAt`). The King Tide reads world.json `events.king_tide` through
`AWRWorldState::Event` and writes `AWROceanState` (stage, start, bore), which `WRTide::Level` already follows.
Secrets are `AWROceanSpot` kind `secret` (`When` / `Needs`); a mooring stone tie-off is `world ocean tie`.
The bosses are `WROceanBosses.*` (`FWROceanBoss`, friend of `AWRBoss`, called beside `FWRFungalBoss`; per-boss server
state in a map keyed by the boss; `AWROceanBossProp` the spare shells and the Pound's breakable towers; `AWRSunMirror`
the three mirrors, `world ocean mirror`; the Pound's flood from `AWROceanState::SluicesBurstAt` in
`UWROceanArenas::FloodLevel`). Boss grabs reuse the sea creatures' hold (`FWROceanAI::StartHold`: pinch, under, drag,
swallow), so a held player's presses mash them like any other.
Under the water the audio director eases `UnderBlend` and low-passes every stream it owns, and `WRSound` low-passes
3D one-shots spawned while the local listener is under; the King Tide layer is `KingTideLayer` (`layer.king_tide`).

Also since 0.7.1 (in progress): **the Sky islands, over the Hollow Cape north of the Storm peaks** (cooks/biome-sky.md,
plan cooks/build-biome-sky.md). One generator: `node Tools/terrain/sky.mjs` writes the cape's ground
(`Data/lands/sky.json` + `sky_ground.bin`: the Windward Cliffs, the downs, the Flue's crown, three camps) and
`Data/sky.json` (44 islands with their shape rows, sites, kit, lifts, bridges, updrafts, gust lanes, hazards, the
Undercloud, resources, the flyers' graph, arenas, the Turning Wind's splines), read by `FWRSkyIsleData`
(`WRSkyIsleTypes`); it prints the island rules (>= 40 m, <= 220 m apart, every island glide-reachable). The Sky's code
name space is `WRSkyIsles` (`WRSky` is the renderer's sky actor). Islands are built in code (`WRSkyIsland.cpp`: domed
top, rim, root, keel crystals, caves) on every machine; places in `WRSkyScenery*.cpp` / `WRSkySites.cpp` on the forest's
FBuild with the Sky's kit `WRSkyLook.h`; `UWRSkyWorld` runs it (lifts, stones, shards, lines, spots, caches). **Air is
game-wide** (`WRSkyAir`): fall damage for everyone (tuning `fall`), gliding is the movement component's `CMOVE_Glide`
(tuning `glide`), every push in the Sky goes through `WRSkyAir::Push` and gust lanes / Gale / rotors are pure functions
of the server time (`WindPushAt`), so they predict. Numbers: tuning `skyWorld`, `glide`, `fall`, `hazards.sky`.
Dev: `world sky ...`. Building the editor target while another project's editor has Live Coding open needs UBT's
`-NoHotReloadFromIDE` (the Live Coding check keys on the shared engine's UnrealEditor.exe).
The Wind path is `WRWindPath` (Momentum, passives, combos, look; Featherfall is the tag `Passive.Featherfall` on the
main path, so ask `WRWind::NoFallDamage`, not HasNode) and `WRAbilitiesWind` (its abilities); `WRSkyAir` calls its
Path Glide hooks (`CanPathGlide`, `PathGlideStats`, `PathGlideTick`, `GliderStaminaScale`, `GlideSinkScale`,
`OnEnteredUpdraft`). Yara Kesh's quest deeds (first_flight, updraft_top, broken_way) are counted in
`UWRSkyAirSubsystem::StepDeeds`; quests can `need` several items (`"a|b"` takes either) and `mark` a place on the map
once done. Found blueprints (`gear.json` kind "piece" / "found" with "from") never drop from portals.
The Sky's creatures: `UWRSkyBody` (flags + look; the Skyray, Cloudback and Sailbell are `UWRSkyBeast` code bodies),
`FWRSkyAI` hooks in AWRCreatureAI (TickWild/RowAllowed/RowStarted/StrikeRow/TickStrike/Resolved, `Damaged` from
`OnDamageTaken`), `UWRSkyLife` spawns them on the islands itself (tuning `skyLife.populations`; the generic wildlife
can't stand things on floating islands). Helpers in these files carry prefixes (`Sl*`, `Be*`, `Sk*`) so the unity
blobs never see two `Tn`s.
The Sky's bosses: `FWRSkyBoss` hooks beside the Graveyard's in AWRBoss / WRBossCombat; `UWRSkyArenas` places their
spawners, the Ring (`AWRSkyRing`: positions a pure function of the server time and `UnboundAt`) and the Wind altar;
the Sentinel's deck holes are `AWRSkyState::DeckOpen` (every machine hides those slabs); riders on Aeolith are attached
to it (`FWRSkyBoss::IsRiding`; Jump lets go through the mash path).
Also since 0.7.1 (in progress, live): **the Crystal caves**
(cooks/biome-crystal.md, plan cooks/build-biome-crystal.md). `node Tools/terrain/crystal.mjs` writes
`Data/lands/crystal.json` + `crystal_ground.bin` (the plateau, on the Wasteland's frame so the Severance runs through
both; `Tools/terrain/wasteland_ground.mjs` is the JS port of the Wasteland's ground it meets) and `Data/crystal.json`
(halls, tubes, shafts, sites, kit POIs, hazards, nodes: read by `FWRCrystalData` in `WRCrystalTypes.cpp`); it also adds its
POIs to `island_dressing.json` (dressing.mjs keeps them). The caves are one signed-distance union (`FWRCrystalData::Air`)
meshed by surface nets in `AWRCrystalCaves`, so what you see, stand on and what `WRCrystal::InCave` answers agree.
Crystal linings glow through `M_CrystalGlow` (`Tools/terrain/build_crystal_materials.py`, a commandlet). Scenery:
`WRCrystalScenery*.cpp` on the forest's FBuild with the kit `WRCrystalLook.h`; built by `UWRCrystalWorld`.
The live half is `WRCrystalThings.*`: `WRCrystal::Emit` is the one noise entry (call it from anything new that makes
a sound in the caves: the server hears it on `UWRCrystalWorld::OnNoise`, every machine's crystals light from it), the
keystone chimes and keepsakes are `AWRCrystalSpot` (E: `world crystal spot`), `AWRCrystalState` replicates the halls'
keystone light. Numbers: tuning `crystalWorld` (noise radii, light, nodes, Saga's night place).
Hazards are `WRCrystalHazards.*` (ward fields + keystones, chandeliers, thin floors, the Silent Vault's door
`AWRSilentDoor` (the hidden-vault door is `AWRVaultDoor`, a different thing), scalding pools, the Resonance); they hear
noises through `OnNoise` and the creatures will listen to `OnPeal`. Numbers: tuning `hazards.crystal`, `crystalEvents`.
The creatures are `WRCrystalCreatures.*` (`UWRCrystalBody` looks and state, `FWRCrystalAI` hooks beside the
Graveyard's in `WRCreature*.cpp` / `WRStormCreatures.cpp`, the dens and the quiet road); numbers in tuning
`crystalLife`. The Arcane path is `WRArcanePath.*` (Mana, shields, Reflect, passives, combos, the look) and
`WRAbilitiesArcane.*` (nine abilities, `AWRArcaneField` runes / Aegis / Rune circle, `UWRArcaneWorld` for the shields'
clocks and films); shields are a shared layer (`WRArcane::GiveShield`, taken in `ApplyHit` after guard): anything new
that shields should use it. `WRArcane::Reflect` throws a projectile back (set `FWRHit::bReflectable` on straight shots).
Hard light (shields, runes, beams) uses `/Game/Terrain/M_ArcaneWard`. Numbers: tuning `arcane`, `spells.*`.
The bosses are `WRCrystalBosses.*` (hooks `FWRCrystalBoss` beside the Graveyard's in `WRBoss*.cpp`; the machines
`AWRCrystalBossProp`: lens, magazine, chime, turntable, lance; `UWRCrystalBossBody` builds their parts and the Twins'
water reflection via `M_MirrorWater`) and `WRCrystalLyrathis.cpp` (her beam `AWRPrismBeam`, dome, copies, phase 3, the
turntables' look). A move that needs the room is in the hooks; numbers in tuning `bosses.<id>`. Not built: composed
music (stand-in pools marked `_standin`).
Also since 0.7.1 (in progress, live): **the Fungal caverns** (cooks/biome-fungal.md, plan cooks/build-biome-fungal.md).
`node Tools/terrain/fungal.mjs` writes `Data/lands/fungal.json` + `fungal_ground.bin` (real metres, priority 1 with
`cuts` over the Sky land's west downs: it reads `sky_ground.bin` so the seam samples are the Sky's own; rerun it if the
Sky land's ground changes) and `Data/fungal.json` (read by `FWRFungalData`, `WRFungalTypes.cpp`). The caverns are the
Crystal lane's design as a fungal twin (`FWRFungalData::Air`, `AWRFungalCaves` surface nets; hall kinds hall / doline /
chasm / grotto / dome, tubes tube / crawl / stair). The rock and every glowing thing use `M_FungalGlow` (vertex alpha =
glow) through `UWRFungalWorld`'s shared instances (`GlowMID`, `VeilMID`, and the rock's own), driven by the surface
clock and `BeatAmount` / `BeatRate` (the bloom and the Sporefall). Scenery: `WRFungalScenery.cpp` (surface, dispatch)
and `WRFungalSceneryCaves.cpp` (halls by zone, sites, the city), kit `WRFungalLook.h` (`FFg` = FBuild + a glow mesh + a
veil mesh). The Flue junction: `WRSkyParts::Flue` skips the wall panel at `WRFungal::FlueMouth`, and the caverns' mesher
unions the shaft's air (radius `flue.r`) and drops its duplicate skin. Things in the caverns stand at their hall's local
z 0 (never `FBuild::G`, which traces to the plateau); walls and ceilings come from the air (`FgWall`). Numbers: tuning
`fungalWorld`. The server's things are `WRFungalThings.cpp` (nodes with their rules, caches, keepsakes; node looks in
`WRFungal::NodeLook`, called first in `WRResourceLook::Apply`). Hazards are `WRFungalHazards.*` (`AWRSporeCloud`: anything new that makes a cloud should spawn one;
`WRFungal::Wind` / `FireAt` / `FrostAt` are the entries for anything new that blows, burns or freezes in the caverns; every
hit already calls `WRFungal::OnHit`, every melee swing `WRFungal::Swing`), doors and secrets `WRFungalSpots.cpp` (E: `world
fungal spot`), the live steps and the Sporefall `WRFungalEvents.cpp`. Creatures are `WRFungalCreatures.*`
(`UWRFungalBody` on every `fungal` creature, `FWRFungalAI` hooked beside the Crystal / Grave ones; the dens are
`UWRFungalWorld::StepDens` over `Data/fungal.json` dens and `denTables`; numbers in tuning `fungalLife`). A new grab
that holds a survivor should answer `FWRFungalAI::Mash`'s chain the same way (`WRCharacterCombat.cpp`). Bosses are
`WRFungalBosses.*` / `WRFungalBossLook.cpp` (`FWRFungalBoss` beside `FWRCrystalBoss` in the boss machine; arena things are
`AWRFungalBossProp`; Mycelorn's clouds are its own (`Maker`), so anything that pushes a cloud through `AWRSporeCloud::Push`
can choke it). The Spore path is `WRSporePath.*` (rules, Spores, planted Spores on `UWRSporeHost`, summons, combos,
the Sporestalker's calm) and `WRAbilitiesSpore.*`; Detonate is granted free with Spore cloud in
`UWRPathComponent::RefreshQuickSlots`; anything new that plants Spores calls `WRSporePath::Plant`. The caverns' sound is stand-in rows in `Data/Audio/sounds.json` (`_standin`;
the fused calls and the cook's 70 BPM cues are the audio cook's), the beds and the Fall Field's silence in
`UWRAudioDirector` (`FungalZone`, `FlueRoar`).

- **The world store is SQLite** (since build-server-raids): `Saved/Worlds/<id>/world.db`, through `WRStore`
  (`ReadDoc / WriteDoc`, `ReadCharacter / WriteCharacter`): a new system that saves per world adds a document kind there,
  never its own file in the world folder. Schema changes are a new `Data/db/migrations/NNNN_*.sql`. One server per world
  folder (the DB is opened exclusive). Backups are in the world's `backups/` folder: to restore one, quit the game, copy
  the backup over `world.db` (delete `world.db-wal` if it's there) and start the world. A player who lost their PC's key
  (`Saved/Identity/key.bin`) gets their name back when the host clears it: open `world.db` in any SQLite browser while
  the world is closed and run `UPDATE accounts SET key_hash = '' WHERE name = 'TheirName';` (the multiplayer cook's
  admin screen will do this in game). Raven's headless test tools read the old JSON files: run them with
  `-wrstore=json`.
- **Steam / EOS identity**: the server binds whatever account id the online subsystem gives (`AWRGameMode::PreLogin`,
  `WRIdentity`). Steam needs a Steamworks app id ($100 Steam Direct): not now. **EOS is free; to turn it on (Aaron):**
  1. Go to https://dev.epicgames.com/portal and sign in with the studio's Epic account. 2. Create Product, name it
  Wildreach. 3. In the product, Product Settings > Clients: Add New Client, policy "GameClient", copy the Client ID and
  Client Secret. 4. Product Settings: copy the Product ID, Sandbox ID and Deployment ID (the Live deployment). 5. Paste
  the five values into a message to the build lane; it enables the EOS online subsystem with them (`DefaultEngine.ini`)
  and the game starts binding EOS ids at once (device-id login, no Epic account needed by players).
- **Player raiding** (`WRRaiding`): every player-side source that hurts a building goes through
  `FWRRaiding::DamagePiece` (it rings the claim's bell and keeps the raid record); creature raids still bite through
  `TakeBuildDamage`. Raid tools are items of category `raid`. **Siege engines** (since cookpass-building:
  `WRSiege.cpp`, models in `WRSiegeLook.cpp`, tuning `siege`): items of category `siege` (`battering_ram`, `trebuchet`,
  `ballista`; ammo `siege_stone`, `ballista_bolt`) stood in front of you by `FWRSiege::UseItem`; `AWRSiegeEngine` holds
  the crew (attached at their posts, movement off on both ends), replicates `FWRSiegeRep` (state + the server time it
  began) and every machine works out the pendulum log, the falling counterweight, the sling, the windlass from it; the
  stone is an `AWRProjectile` of kind `trebuchet` (`FWRSiege::StoneLanded`), the bolt kind `ballista`; damage to pieces
  through `DamagePiece` (sources `Ram` / `Siege`), to engines through `AWRSiegeEngine::TakeHit` (melee, shots,
  `SplashPieces` blasts); saved in the world store's `siege` document; a harnessed hauler (`AWRCreature::HarnessedTo`)
  walks behind its ram. Necromancers raise the dead through walls since build-biome-graveyard (`WRAbilitiesNecro.cpp`). The ladder has no climb
  clip; the climbing pose is built in `WRHeroAnimInstance` (hand over hand in time with the rungs, tuning `anim.climb`).
  Every path's area blasts hit buildings through `FWRRaiding::SplashPieces` (Thunderclap, Arcane blast, Starfall,
  Corpse explosion, Collapse, Maw of the void, the Sunburst column, Blight's rot), so `raiding.paths` multipliers apply. The code breaker waits for the Crystal caves (resonant crystal).
- **Bounties** stay a setting only (nothing reads it) until the PvP pass. **52 perks** wait for their systems
  (`waitsFor` in `Data/lifeskills.json`: diving, bees, timed brews...; Siege engineer is live since cookpass-building; Siege-proof and Silent charges are live since build-server-raids; Breeder, Rare finder, Saddle master and
  Creature armorer are live since build-taming; Decorator, Turret tech,
  Master builder and Ancient wood are live since build-building).
- **F8 pictures from the game don't reach the web inbox yet**: the feedback endpoint keeps screenshots (and context over
  4000 characters) for signed-in players only, and the game isn't signed in. The full report and picture are in
  `Saved/Reports` on the tester's PC (and the host's). Text fields (chat, names, the report note) need a keyboard.

## Two people, one repo

The repo is https://github.com/RavenArcher17/Survival-Game (public). Everyone works on `master`.

- **Pull before you start, and again before you push** (`git pull`). Push each change once it builds and its tests
  pass. Don't leave finished work sitting unpushed: the other person builds on master.
- **Commit your own files by name** (`git add <paths>`), never `git add -A` / `git add .`. Raven's PC has a local
  `Config/DefaultEditor.ini` that stays out of git, and the Fab packs and the Electric Dreams forest kit must never go
  in: the repo is public and their licences don't allow sharing them (the kit went in at 29d9fb7 and came out again
  the same day, Raven 11-18).
- **Maps and `.uasset` files can't be merged.** If both of you change the same one, one person's change is lost. C++,
  `Data/*.json`, scripts and docs merge as usual. Before changing a map (`Content/Maps`, `Content/__ExternalActors__`)
  or an asset someone else may be working on, say so; one person at a time.
- **One release at a time.** Agree the version number with Raven first (check `gh release list --repo
  RavenArcher17/Survival-Game -L 3`), so two people never publish the same number.
- **Art goes through Git LFS** on Raven's GitHub account: every clone and pull counts against its LFS allowance.
  Don't re-clone for fun, and say before adding a lot of new art (a creature is about 11 MB, an island rebuild ~80 MB).

## Building and testing

`README.md` has the build command and every test. The ones you'll use most:

```
$env:UE_SKIP_UBT_SDK_SETUP = '1'                       # always, or Unreal waits on any other build on the PC
Tools\alltests.ps1                                     # build, then every test (~90 min), PASS / FAIL table
Tools\alltests.ps1 -SkipBuild -Only door,clan          # just some
Tools\play.ps1 listen                                  # play it yourself from the editor build
```

- Tests drive the real game with bot commands: `-wrbot -wrbotcmds="cmd|cmd|..."` (`WRClick <button text>`,
  `WRKey <key> [hold <s>]`, `WRUIText`, `WRUIState`, `WRTeleport`, `WRGive`, `WRCmd <server command>`, `waitchat <text>`
  to keep two bots in step, `WRWalkTo <x> <y> [radius]` (cm; logs arrived / stuck) with `waitwalk <max s>` to wait for
  it; prefixes `@1:` / `net:` / `solo:` pick which world a command runs in). The bot runner and
  `waitchat` are in `Source/Wildreach/WRPlayerController.cpp`, the UI commands (`WRClick`, `WRKey`, `WRUIText`,
  `WRUIState`) in `WRUIDrive.cpp`. To write a test, copy one (`Tools\doortest.ps1` is a short two-player one).
- Two-player tests run a host with no window (`-nullrhi`, it still has the UI) and a rendered friend.
- **Known flakes**: `menus` and other two-window tests sometimes hit a GPU hang (`DXGI_ERROR_DEVICE_HUNG` in Nanite
  shadow culling); `updater`'s dropped-connection step can fail while the PC is busy. Rerun a failure alone
  (`-Only <name>`) before believing it.
- Runtime errors go to the log, not the exit code: check logs for `Error:`.
- Look at visual changes from at least two camera angles; one angle has hidden real defects before.

## Releasing on Freedom35 (0.8.2 on; lane wild-f35)

Wildreach is listed on Freedom35 Studios (https://freedom35studios.pages.dev/g/wildreach, Raven's studio, under the
Play Pass). Its download is a small installer (F35Update + `f35-update.json` with `"install": "files"` + the icon): the
first start links the PC to the player's account, downloads every file of the live version and checks each SHA-256;
later starts fetch only the files that changed. The site's side is in the freedom35-studios repo (DEPLOY.md
"Wildreach: a download installed from its files", `db/wildreach.sql`).

1. Package Shipping for Freedom35: `Tools\package.ps1 -Store freedom35 -Out <folder> [-Engine <UE_5.8>]` (Shipping is
   the default; the `.pdb` goes to `<folder>\Symbols`). Every file is under 64 MB but the exe, which goes up in parts.
2. Playable gate on the package: start
   `Wildreach\Binaries\Win64\Wildreach-Win64-Shipping.exe -F35Boot -abslog=<file> -windowed` twice: the file must show
   `F35READY menu`, `F35BOOT new`, `F35READY play`, then `F35BOOT continue` and `F35READY play`. (Shipping writes no log
   of its own; these lines are written for the check.) Shipping saves under the account's real
   `%LOCALAPPDATA%\Wildreach\Saved` whatever the LOCALAPPDATA variable says (Windows' known-folder path), so the check's
   `Boot_check` world lands there: delete `Saved\Worlds\Boot_check` afterwards on a PC whose player uses the game.
   Then close the game with its window's close button: the exit code must be 0 (F35Update reports any other code as a
   crash; 0.8.2.1 and earlier ended every quit in 0xC0000374, see CHANGELOG 0.8.2.2).
3. In the site repo, with a developer token for Raven's studio in `F35_TOKEN` (the operator mints a two-hour one):
   `node cli/f35.mjs installer <package>\Windows wildreach --name Wildreach --exe Wildreach.exe --version <v> --icon
   <Wildreach.ico> --files-only --record` -> `node cli/f35.mjs download <setup> wildreach --platform windows --version <v>
   --notes "..."` -> `node cli/f35.mjs files <package>\Windows wildreach --platform windows --version <v>`. Only the
   third step makes the version live for installed copies.
4. `curl -s https://freedom35studios.pages.dev/api/games/wildreach/latest?platform=windows` says the new version with
   `"files": true`.

## Releasing an update

Only with Raven's OK. Copies installed from a GitHub release update themselves from the newest GitHub release; copies
installed from Freedom35 update only through Freedom35 (above). **Until Raven says otherwise (Raven 2026-11-17), every
version goes out on both**: the Freedom35 package (`-Store freedom35`) and a GitHub release made with the steps below
(`-Store github`, the default), with the same `ProjectVersion`, so players on either can play together.

The packaging PC needs the engine's **MetaHuman Creator Core Data** option (Epic launcher > Library > the arrow beside
5.8 > Options): the hero grooms' bindings point into it, and without it the cook fails with "The binding asset (...) has
missing cooked platform data". `Tools\check_setup.ps1` FAILs without it.

1. `git pull`, and make sure the working tree is clean apart from files that stay local.
2. Bump `ProjectVersion` in `Config/DefaultGame.ini` (it's also the network version: different versions can't play
   together). Write `Docs/release-notes/v<ver>.md` (copy the last one's layout) and the `CHANGELOG.md` entry.
3. Run the whole suite; rerun failures alone.
   The PC that packages needs the Electric Dreams forest kit (`Tools\check_setup.ps1` says PASS for it): without it the
   package ships the Ancient forest's tree-kit look. `Tools\art\electric_dreams_forest.ps1 -From <an Electric Dreams
   project>` copies it in (the project: Fab, "Electric Dreams Environment", Create Project in the launcher), then run
   the commandlet the script names (caps its textures at 2K).
4. Package into a **new** folder (a running packaged game locks its folder):
   `Tools\package.ps1 -Out C:\Dev\WildreachBuild<N> -SkipEditorBuild`, then
   `Tools\make_installer.ps1 -Build C:\Dev\WildreachBuild<N>` (makes the whole
   `Wildreach-Setup.exe` there, ~3.6 GB at 0.8.0, and `release\`: what goes on the release).
   GitHub takes no file over 2 GiB, so a bigger Setup goes up in parts: `release\` then holds `Wildreach-Setup.001`,
   `.002`... (1,900 MB each), their list `Wildreach-Setup.parts`, and as `Wildreach-Setup.exe` the small download program
   (`installer\WildreachBootstrap.cs`) that fetches the parts, checks the whole SHA-256 and runs the Setup. Updaters from
   0.8.0 read the parts themselves (`SetupSource`, still only what changed); 0.7.1's and older find no index on the
   download program, fetch it whole and run it, so they still update (the whole game, once).
   Before a big release, refresh the bundled shader cache first (Docs/PERFORMANCE.md "PSO cache": play the package
   with `-logPSO`, `Tools\perf\pso_cache.ps1 -Build <it>`, commit the `.spc`, package again).
5. Test the installer on a copy, never the build folder: the whole `Wildreach-Setup.exe` (not `release\`'s download
   program, which reads the published release) `--silent --nolaunch --noshortcuts --dir
   C:\Dev\WildreachTest`, then start that copy's `Wildreach.exe` and play a little (host a world, open the menus). The
   first start of a game in a new folder brings up a Windows Firewall prompt; answer it yourself.
   The boot check does the first part with nobody at the keys: start that copy with `-F35Boot -log -windowed` (or the
   site's `node tools/bootcheck-pc.mjs wildreach --game-dir <copy>`). Its log must show `F35READY menu`, then
   `F35BOOT new` and `F35READY play`; start it again the same way for `F35BOOT continue` and a second `F35READY play`.
   An engine outside `C:\Program Files\Epic Games\UE_5.8`: pass `-Engine <its UE_5.8 folder>` to both scripts.
   `build.json` beside the game names the commit it was packaged from.
6. Commit (by name) and push. Then publish **every file in `release\`**:
   `gh release create v<ver> C:\Dev\WildreachBuild<N>\release\* --repo RavenArcher17/Survival-Game
   --target <full 40-character commit id> --latest --title "Wildreach <ver>: <what's new>" --notes-file
   Docs\release-notes\v<ver>.md`. A short commit id gets HTTP 422.
7. Check it: `gh release view v<ver> --repo RavenArcher17/Survival-Game --json assets` shows a `digest` for each file
   that must equal `(Get-FileHash <that file in release\> -Algorithm SHA256).Hash`, and https://github.com/RavenArcher17/Survival-Game/releases/latest
   must lead to the new tag. Then mark the CHANGELOG entry released, commit, push.

## Traps

- **Unity builds**: Unreal joins `.cpp` files into one, and adding a file reshuffles them. A local with the same name as
  a global or member elsewhere then breaks the build (C4459 / C4458 / C4456 are errors here). Keep file helpers in
  named namespaces, and don't reuse short global names.
- **Screen sizes**: the UI lays out on a canvas at least 1760 x 1080 logical px (`UWRDPIScalingRule`, `WRUIFit.h`), and
  every viewport root sits in an `SWRFit` (`WRUI::Fit(...)`) so nothing runs off a small screen. A new full-screen
  widget added with `AddViewportWidgetContent` goes in one too; place things by flow (boxes, anchors), never by fixed
  offsets from the top left. Keep any window under 1728 x 1000 logical px.
- **Update now** must never open a web page: it starts `WildreachUpdater.exe` (beside the game, up from the exe, or the
  installed copy's) or downloads the release's Setup and runs it with `--update --wait <pid> --dir <folder>`.
- **Full-screen UI widgets must be `SelfHitTestInvisible`**, or an empty layer eats every click (0.4.0 shipped with no
  menu taking a click). Only tests that click for real (`menus`, `base`, `clan`...) catch it.
- **Anything loaded by path** (creature art, dressing meshes) must be under a `DirectoriesToAlwaysCook` entry in
  `Config/DefaultGame.ini`, or the packaged game won't have it.
- **Adding a Fab pack**: a "complete project" pack can overwrite `Config/Default*.ini` and drop a `.uproject` in the
  root. Run `Tools\check_setup.ps1` (or `git diff -- Config`) after adding any pack.
- **The Barghest pack's mane and tail** (`SK_BARGHEST_MANE`, `SK_BARGHEST_TAIL`, `BARGHEST_MANE_Anim`,
  `BARGHEST_TAIL_Anim`) crash cooking (`Assertion failed: !Hash.IsZero()`). The game doesn't use them; move them out of
  `Content` after adding QuadrapedCreatures. `check_setup.ps1` finds them.
- **GPU hangs**: Nanite tessellation is off (`r.Nanite.Tessellation=0` in `DefaultEngine.ini`) because it hung the GPU;
  don't turn it back on. The same goes for the shrubs' ash coat.
- **PowerShell 5.1**: a `.ps1` with non-ASCII characters needs a UTF-8 BOM; config and JSON files must not have one
  (write them with `[IO.File]::WriteAllText`, not `Set-Content -Encoding UTF8`). `git commit -F -` with a here-string
  fails in PowerShell: write the message to a file and use `-F <file>`.
- **Headless clients** need `t.MaxFPS 60` or they flood the server. Use `?WRName=<name>` for a stable player name.
- **Server presets**: with no settings a server plays `pve` (normal XP, Raven 11-18). Every test passes
  `-wrdefaultpreset=playtest` (10x XP, taming 10x, PvP off) beside `-wrstore=json`, which only fills in for a world with no
  settings of its own; PvP tests pass `-wrpreset=pvp_standard`, which overrides a world's settings.
- **The island's landscape** rebuilds only in the full editor (`Tools\terrain\build_island.ps1 -Force`), not a
  `-nullrhi` commandlet. Trees, rocks, caves and hazards change without a rebuild: `node Tools/terrain/dressing.mjs`.

## Art

- Creatures and props came from a local AI pipeline on Raven's PC (SDXL concept, Hunyuan3D 2.0 shape and texture,
  Blender rig; `Tools/art/README.md`). Its environment and models (`C:\Dev\WildreachArt`) aren't in git; what the game
  uses is imported under `Content/Creatures` and `Content/Props`, which are.
- Hunyuan3D 2.0's licence doesn't cover the European Union, the United Kingdom or South Korea, so nothing made with it
  is licensed there.
- The humanoid bosses and the player were rigged on Mixamo with Raven's Adobe account.
- Outside files and their licences are listed in `Docs/CREDITS.md` (shipped beside the game). Add a row for anything
  new from outside.
- **The hero** is made in the hero lab (`C:\Dev\WildreachCharLab` on Aaron's PC: MetaHuman Creator, the outfits, the
  retargeting in `Scripts/anim/`). Only built output comes here: `Content/Wildreach/Characters/Hero/{Unpacked,Outfits,
  Materials,Anims}` (2K textures, cards hair, no editable `MH_*` sources). The skeleton and the face rig are the
  MetaHumanCharacter plugin's (enabled in the `.uproject`, ships with 5.8), so they aren't in git. `Data/player_art.json`
  (schema 2) says which body, face, grooms and outfit each sex wears, the sockets, the action clips by key and the
  motion-matching chooser. After new clips arrive, run `Tools/anim/build_hero_anim.py` (the command line is in the
  file) to rebuild the databases from `Tools/anim/hero_databases.json` and the combat windows from
  `Data/anim_windows.json`.
- **The hero's look** (`WRHeroLook.cpp`, all data in `Data/player_art.json`): `hero.<sex>.faces` (head, its skin for
  every build, default grooms, hair colour, every groom option), `builds` (lean / athletic / strong body meshes),
  `looks` (4 slot meshes per build, body-hide masks per slot, `hidesHair`, material swaps), `armorLooks` (item id ->
  look, by element), `elements` (trim colours), `paths` (skin / eye / hair parameters per path). Grooms are the
  MetaHumanCharacter plugin's own assets with a small binding per face (`Hero/Grooms/<face>/`): never copy groom assets
  into the repo (they carry strand data; F1's was 63 MB). Lab scripts that make all of it: `Scripts/hero2/` in the lab
  (faces via `faces2.sh`, armour via `hero_armor.py`, masks via `gen_hide_masks.py`, migration via `migrate.sh`).
- **Generated meshes** (11-22): `Tools/art/make_meshes.py` builds shapes the packs lack with Geometry Script
  (`unreal.GeometryScript_Primitives` / `_MeshDeformers` / `_NewAssetUtils`) into `/Game/Art/Gen` (in git, cooked:
  DefaultGame.ini): basalt column clusters, obsidian blades, seracs, the sulfur crystal, lava rubble patches (Nanite),
  the camps' ridge tent (`EWRLookMesh::Tent`, drawn in M_BuildTinted like any block), the Ice Arch, the pingo, ice
  blocks and hornitos (placed by `FWRIsleSite::Shape` at their own size in M_IceBlock or the dressing's rock; null
  when the mesh is missing, so a place keeps its look-kit build). `Raw` builds a mesh from rings of points (each
  face wound by an outward hint), `fbm` is its noise. `WR_MESHES=basalt,serac` makes
  only those. Materials come from where they are used (world-space projected), so the meshes need no real UVs. Its
  `xf()` takes rotations as (pitch, yaw, roll): Python's `unreal.Rotator` is (roll, pitch, yaw). Raw triangles: the
  DynamicMesh's front is cross(c - a, b - a) (`tent()` checks its first face).
- **Generated meshes, round 4** (11-22): the bones, the longboat, the spire, the Glasswall and the quarry are built from
  helpers in `make_meshes.py`: `prism()` (faceted columns: each side its own strip, so edges stay sharp), `tube()`
  (rings swept along a path), `ellipsoid()`, and for bone `voxel()` (Geometry Script solidify + a morphological
  close, so parts grow into each other) and `boolean()` (the skull's sockets). Look a mesh over before the game:
  `WR_OBJ=<folder> WR_OBJ_ONLY=1 WR_MESHES=skull` writes OBJs, then render them in Blender. In C++,
  `FWRIsleSite::ShapeKit` draws a mesh in M_BuildTinted (one instance with the kit's custom data: colour, pattern,
  grain axis) and `ShapeMat` in a named material.
- **The light** (11-22, round 5) is all `AWRSky` + `tuning.json` `sky` (`_light` says what each key does): golden hour
  tints the map's sun (`goldenSunColor` x `goldenSunTint` by `FWRSkyRules::LowSun`), night grades toward
  `nightWhiteTemp` / `nightSaturation` and darkens the round island's moonlit sky (`nightSkyLuminance`, the sky
  atmosphere's luminance factor, folded in with the weather's tint), exposure biases by daylight add to the map's own,
  and by day local exposure (`dayHighlightContrast` / `dayShadowContrast`) keeps dark ground under a bright sky. Look
  at it with the scratch spots at 12, 19.4 (the sun ~9 degrees up: 18.5 is still 21 degrees, not golden) and 23.
  Hearthmoot's lights are `roundIsland.<kind>Light` / `<kind>LightRadius` (StepLights).
- **Launches that must carry sideways** (a geyser onto a ledge): call `UWRCharacterMovementComponent::Carry(seconds)`
  after `LaunchCharacter`; with no input the survivor's braking friction in the air kills any sideways launch in ~20 cm.
- **Shores** (11-22 round 6): wherever ground meets water, give it one continuous profile through the water's level at
  the outline (signed distance); a step between two 4 m samples there makes the waterline follow the grid.
  `round_ground.mjs` SEA_LEVEL (the sea's -0.2 m) and the lake's profile do this.
- **Cliffs**: `fire_frost_dressing.mjs`'s cliff pass places `granite_cliff` panels on the Tundra's steep rock (not
  its ice) by the area of the face (CLIFF_AREA m2 each; spaced over the map, a sheer wall got a row every 40 m up it),
  the point on the face and the yaw facing down it; `WRWorldDressing`'s FArt `Laid` (> 0) sets the panel there by its
  middle and leans it back that share of the way to the slope measured on the ground (WRLand::GroundZAt). The cliff
  pass has its own clearings (16 m from places, 32 m from the Glasswall). Meshes: `make_meshes.py` crag() (SM_Crag_A..D,
  soft=True SM_CragSoft_A..D). The steep ice grows its own walls (WRLandCaves, below); the Volcanic's steep ground is
  carved gullies and layered rock (no panels: laid close up every ridge they read as brick walls). The Old Island
  still lays `basalt_cliff` and `ice_cliff` (SM_IceFace, FArt `bTall`) from its own dressing file.
- **The coast** (11-22 round 7): `fire_frost_dressing.mjs`'s coast pass dresses every land's beaches and the meadow lake's
  banks: `beach_pebbles` / `black_pebbles` / `frost_pebbles` (SM_Pebbles, by the land), `wrack` (SM_Wrack, along the
  shore's tangent), `driftwood` (SM_Driftwood, collides), `dune_grass` (the MW pack's grass, pale), `shore_ice` (the
  Tundra). Heights are against the sea's level (-0.2 m) or the lake's; camps, the jetty, the lake's beach and reeds keep
  clear; heights are from where the sea's edge is drawn (~+0.75 m: its material lifts and waves it), not its level.
  FArt's `bOrigin` (origin on the ground, Size zero = the mesh's own size), `bFollow` (tipped to the ground's slope),
  `bShadow` and `Tinted` (M_BuildTinted's ten per-instance floats; shade and grain axis set per instance) carry them.
- **The moon on the lake**: the lake wears the WaterMaterials ocean (translucent, lit through the volume: no light's
  highlight ever shows on it). `AWRRoundIsland` lays a second sheet over it in M_WaterGlint (`build_terrain_materials.py`
  WR_MATERIALS=glint: additive, the view reflected off six wave trains against LightDir), fed by `AWRSky::GetMoonDirection`
  / `GetMoonGlint` each tick (roundIsland.glintStrength / glintLevel / glintSharp / glintRipple).
- **The wet band** (`wetShore`: sea, shown, lake, dark): measured up from `shown` (where the sea's edge is drawn). The
  island's ground takes its brightness from its textures, so the vertex colour hardly darkens it: the wetness also goes to
  the ground's first UV (`HfWet`), which `M_IslandGround` (`build_forest_ground.py --island`) treats as rain.
- **The town's look shapes** (round 8): hearthmoot.json blocks' mesh 5 awning, 6 haystack, 7 rowboat are generated
  meshes (make_meshes.py, WR_MESHES=townprops) fitting a 100 cm box like the engine cube (`EWRLookMesh`, WRBuildLook
  MeshFor; each falls back to the shape it replaced). New details in hearthmoot.mjs draw on their own random streams
  (RW windows' glow, RS back and end windows) so the town doesn't reshuffle. The fields' crops are generated clumps
  (WR_MESHES=crops) drawn by StepCover in M_BuildTinted.
- **Ground painting and the meadow**: round_ground.mjs paints the Volcanic's and the Tundra's sub-zone surfaces only past
  the meadow's edge (their wedges reach to the walls). Any colour noise finer than about two samples (8 m) aliases into
  stripes on the 4 m ground: keep ground noise scales at 13 m or more.
- **Moss by water**: WRLand HfLayers mosses dry ground beside a bed, bank or marsh; a land's dampMoss [reach in samples,
  most] sets how far and how much (default 2 and 0.7; the round island 1 and 0.25).
- **Town clothes' slots**: on the wardcloth and gravecloth cuts the "Coat" slot is the shirt with its sleeves, the
  "Bodice" / "Shroud" the panel in its neck (and, on the legs, the hose: "Legs/Ward_Bodice").
- **The forts** (round 9): hearthmoot.mjs fortAt(t) builds each fort by its road's lands (FORT_STYLE sand / granite /
  shore / basalt) on its own random stream (RF); the forts' watch joins the city's people last (fortGuards), so the
  town's draws don't move. A stone block with a palette colour is tinted stone (WRRoundIsland sets bStoneTint; M_BuildStone
  reads custom data 3-5, white otherwise): other callers' stone is unchanged.
- **The stream** (round 9): round_island.mjs draws it every 12 m with its meanders; round_ground.mjs cuts its channel
  (bed to 3.5 m, a muddy edge to 5.5 m, the meadow's grass beyond); its pebbles come from fire_frost_dressing.mjs's coast
  block (LAND.lake.stream, with each point's water level), its reeds from hearthmoot.mjs (RD).
- **Road verges**: a main road's bed is still 12 m (round_ground.mjs TRUNK_W) but its gravel only ROAD_TRACK of it; the rest
  is surface 58 `verge` (WRRoundIsland StepCover grows thin grass there).
- **Soft surfaces** (round 10): a land file's surface with `soft` 1 is an area, blended over a ragged ring of nine
  samples `surfaceBlend` m round each sample (WRLand HfSoftRing, in HfGround and HfLayers); others (roads, fields,
  cracks, beaches) keep the old sharp edge. The round island: 7 m. Grass cover asks its kind `coverEdgeJitter` m off.
- **The lake's water** (round 10): `roundIsland.lakeMaterial` is the pack's M_Water_Opaque (opaque: Lumen reflects into
  it), lakeLift 0, colours in lakeVectors (sRGB by parameter name). The sea's MI_Ocean needed lakeLift 100.
- **Shoes** (round 10): a look's `shoes` {mesh, material, ankleToBall} (on its legs piece's look) puts a static mesh on
  foot_l/foot_r, placed from the body's bind pose (ankle over the ground, toe toward ball_l/r, scaled by that length over
  ankleToBall, 14 cm for SM_Shoe_A).
- **Hair** (round 11): the MetaHuman grooms' bindings have no source mesh, so binding only projected each hair root onto
  the nearest point of our faces, which are reduced meshes (19k vertices, not the template's 34.6k: no transfer is
  possible) whose crowns stand ~11 cm higher and ~10% wider than the template head the grooms were made on (the women's
  only 1-2 cm). Every man's beard hung at his throat and his hair at his nape. WRCharacter now hangs each groom on the
  body's head bone, unbound and rigid, moved from the template's crown (hero.groomTemplate, SKM_Face's bounds) to the
  face's crown and widened to its width; eyelashes (made in another space) are hidden. hero.groomFit 0 = the old
  binding. Rebuilding the bindings with MH_Groom_Head as source threw the hair 20 cm up (the vertices don't match).
  Not only the lashes: every woman's hairstyle and some brows and lashes are made round MH_Groom_Head (eyes at z 149.3,
  not SKM_Face's 160.4): a groom whose own top is under its slot's split (hair 167, moustache 151, others 156.5 cm) is
  raised hero.groomLowShift first; the eyelashes then fit by the eyes (hero.lashTemplate, SKM_Face's eyes).
- **Villages** (round 12): AWRVillage::BuildLook places its huts in two loose rings (odd ones 1.55 x ring), each
  hut's size and offset hashed from its index (K.Jit/K.Rand), households by the doors, a boundary wall (basalt) or
  windbreak (lavvu) at 2.15 x ring round the back (95-265 degrees); the round island's villages have huts 8
  (fire_frost_dressing.mjs). The Tundra's frozen lakes are ground (lake_ice), not the Old Island's ice sheet (none is
  drawn on the round island).
- **Worn ground** (round 13): `WRGroundTrail::Path` and `::Patch` lay a ribbon or disc of procedural mesh over a live
  heightfield land in the land's own material (`AWRLand::GroundLook`). Each vertex carries the land's colour, layers
  and wetness from its four samples, moved toward an `FWRTrailLook` (`Ash()`, `Snow()`) by the wear, so the edges
  are the ground's own. Nothing is drawn off a heightfield land (the Old Island). M_IslandGround reads UV0.x as the
  shore's wetness: anything drawn in it must put the land's `HfWet` there, not map coordinates (the cave lips did,
  and drew as wet ground).
- **Collars** (round 13): `Tools/art/fix_collars.py` reshapes Rimeguard's Rime_Fur ring and Dunewrap's Dune_Linen
  ring on every build and LOD. Run it only on the lab's meshes: a second run deforms them again, so `git checkout`
  the meshes first.
- **The Crevasse Maze** (round 14; rounds 12-13 stood rows of ice on a fitted plane, SM_CrevasseRow_0..5 and
  SM_CrevasseFill_A, now unused): the crevasses are cut into the ground. `round_ground.mjs` fits the slope under the
  place, lays five slots down it (land file `crevasses`: the field's frame U/V and each slot's middle, half length,
  half width, depth and end ramp, m), cuts the ground over the whole field (HfHidden) and adds each slot to the
  `underground` ways (a body on a crevasse's floor is rightly under the ground). `WRLandCaves` draws the field: the
  slots' air (`LcSlotAir`) is carved like a cave's, the ground between them is the skin in the land's own look.
  CrevasseMaze lays the bridges across the slots from the land's data (`FWRLandDef::Crevasses`), the poles, ice
  fallen in and the seracs. The bridges are SM_SnowBridge_A/B and SM_IceBridge_A with box collision; each snow bridge
  is a hazard whose Whole is its mesh. Its column test (WRIslandHazards) is straight down, whatever the bridge's tilt.
  The kit's S.R / S.J hash 5 cm cells of the place, so seed them with points a metre apart (Rn / Jn), never raw
  indices.
- **Ice walls** (round 14): the Tundra's steep ice (glacier, ice, crevasse and lake ice samples steeper than
  landCaves.wallFrom..wallTo, 44-56 degrees, in a Tundra zone) grows out of the land in `WRLandCaves`: the rock's
  field takes the ground pushed out of the face (`LcWallPush`: 0.7-1.9 m, ribs top to bottom, set back in steps),
  so a face stands proud of the land in fluted ice and its top and foot roll into the ground; coloured as weathered
  ice with the strata faint (`WIce`), snow on its ledges. (The SM_IceFace panels they replaced read as rows of striped
  slabs; that pass is gone from fire_frost_dressing.mjs.) landCaves.walls 0 turns them off
  (-wrtune=landCaves.walls:0). Cost, measured in the editor's game at 40-54 degrees: ~1,600 more cave meshes, +2.7 GB
  on a client and +1.9 GB on the server (each procedural mesh ~1.7 MB with its collision); frame times stayed at the
  60 fps cap. Cubes that stand in for cut ground (caves, crevasses) are built before any wall. Grown the same way, the
  Volcanic's steep ground in rock cost +12 GB on a client for little on its dark ridges, and was taken out: its steep
  ground is carved gullies (round_ground.mjs) drawn as layered rock (M_IslandGround). Growing walls more widely wants
  baked Nanite meshes, not procedural ones.
- **Cuts in the land** (round 14): a land chunk hangs a 0.9 m skirt round every cut square (a cave's mouth, the
  maze), and the caves' skin is dropped only where the land draws all round it two cells out: either alone left a
  hairline along a cut's edge.
- **Steep rock** (round 14): round_ground.mjs carves gullies into the heightfield on every Volcanic and T1/T2/T7
  sample steeper than 34-44 degrees (26 m apart, up to 4 m deep; not within 70 m of a place or near the city), before
  the caves are laid. M_IslandGround's steep-rock bands and ribs multiply the FINAL colour (after the land's tint);
  multiplied into the texture before it, they were clipped away (the tint takes only the texture's brightness, capped).
  On dark ground the auto and local exposure even out broad colour changes: relief has to come from the ground's shape.
- **Clear ice** (round 14): M_IslandGround draws ground whose own roughness is under ~0.2 after the soft blend (lake
  ice 0.06, the cirque's polished ice 0.08) as black ice: glossy, the textures' grain gone, cracks and bubbles at
  odd angles and sizes (at one square size they tiled into a grid from the air).
- **Outfit meshes** (round 11): the lab's outfits came with parts wound inside out (270 of 408 meshes);
  Tools/art/check_outfit_winding.py checks them (WR_FIX=1 turns them). Run it on any outfit mesh brought in. Trim (vertex
  colour R) blends by the base materials' TrimAmount (0.4, Tools/art/fix_outfit_trim.py); per-material looks in
  Tools/art/tune_outfit_materials.py. A bot can wear a set: WRGive <item> 1 then WREquip <item> (8 sets a game).
- **The Survivor Wraps** (round 11): all trim on M_WR_Garment (TrimColor is their colour); the look's materials map
  gives each part its own (Tools/art/make_wraps_look.py). Their kilt was wound inside out; Tools/art/fix_wraps_kilt.py
  reversed it (it checks first: running it again leaves it). The sword and shield wear M_PropMetal (metal found by colour). Far ground (WRLand BuildLodChunk): a far vertex over any bed (class
  1) takes the bed's lowest sample in its square, so narrow channels stay carved at 1/2-1/8 detail.
- **NaN vertices**: `FMath::Pow(FMath::Sin(PI * t), e)` at t = 1 is NaN (sin(PI) is a hair below 0 in floats), and a NaN
  vertex gives the component NaN bounds (an ensure). Clamp the sine: `FMath::Pow(FMath::Max(0.f, FMath::Sin(...)), e)`.
- **Building materials** (`Tools/make_build_materials.py`, `make_build_materials_tinted.py`): stone is SimpleRocks'
  rock in courses; wood is WoodMaterialPack2's scans set at run time (`FWRBuildLook::Material`; optional pack, the
  procedural planks without it); M_BuildTinted patterns 0 stone, 1 wood, 2 metal, 3 smooth, 4 plaster, 5 pantiles,
  6 thatch, 7 shingles, 8 slate, 9 cloth, 10 ice. Detail blocks are mapped in their own frame (instance space,
  `block_frame`: custom data 1 is the world axis the long side lies nearest, plus 3 for a cylinder or cone, whose
  texture then runs round it); a block whose middle side is over ~1.6 m draws joints (courses, boards). "Local" space
  on an instanced component is the component's: use Instance space. A material a C++ constructor loads at startup can't
  be rebuilt by these scripts ("Assertion failed: !IsRooted()" in DeleteAllMaterialExpressions): load building
  materials at play time.
- **Hero faces:** `Tools/art/fix_face_lod3.py` gives a face's LOD3 and LOD5to7 skin materials their LOD1 textures when
  they are missing (F1, F2, F3, F6, M3 came without at LOD3 and went black at 4-12 m; all twelve lacked "SRMF Baked VT"
  at LOD5to7). Run it again after importing new faces. If skin ever draws shiny black everywhere (face and body alike),
  look at MF_PathMarks first: its code node must read the BaseColor input M_WR_Skin feeds (`Tools/art/
  fix_pathmarks_input.py` mended a function built twice, 11-22). Tuning overrides on the command line are
  `-wrtune=key:value,key:value` (a colon, not =).
- The hero animates with **no animation blueprint**: `UWRHeroAnimInstance` (a `UWRAnimInstance`) does motion matching
  in C++ (`UPoseSearchLibrary::MotionMatch` with its own pose history and blend stack), because the engine's
  motion-matching node can't run outside an animation blueprint in a packaged build and anim graphs can't be made
  headless.
