# Wildreach cook: COOKPASS-CRAFTING (crafting, stations and machinery: the cook pass on what's built)

Lane cookpass-crafting, 2026-11-15/16 night. A cook pass on what the build lanes landed for crafting (Raven's stations,
queue and Hearth; `cooks/build-crafting.md` 9e… steel / starsteel / sets / runes / salvage / blueprints / rings /
talismans), stations (forge, armorer, alchemy table, workbench, charcoal kiln, bloomery) and machinery
(`cooks/build-machinery.md`). Research how the best games do it, grade every built piece against THE BAR, design the
raise, build it. Spec sources: FULL-GAME 6.3, `cooks/survival-crafting.md` 2.12-2.17, the LATER-COOKS lines the crafting
and machinery lanes left (7, 11, 17, 121, 140-150).

## 0. Requirement ledger

| # | Requirement (one each) | Source | Done where (filled at the end, section 5) |
|---|---|---|---|
| Q1 | Research how the best games do crafting, stations and machinery (named games, places, systems) plus real-world reference; written here | brief, Aaron 11-13 | |
| Q2 | Grade every built piece against THE BAR | brief | |
| Q3 | Design the raise in writing (every detail, edge cases, build steps) before building | brief, Aaron 11-13 | |
| Q4 | Build exactly the plan at full detail; no placeholders; unfinished stays hidden | brief, BAR | |
| Q5 | Spot-check against the plan and THE BAR by reading the code; fix before push | brief | |
| Q6 | "Already done" needs file:line proof | brief | |
| Q7 | Design with intention: every object says why it is here, what it connects to, how it meets what it touches; nothing passes through anything | Aaron 11-14 | |
| Q8 | Moving parts move continuously and smoothly on a believable mechanism; nothing snaps or teleports | Aaron 11-14 | |
| Q9 | Data-driven: numbers in Data/*.json / tuning; sounds and fx as rows; damage through ApplyHit; server authority | brief, BAR | |
| Q10 | Mouse AND pad on every screen touched | BAR | |
| Q11 | Multiplayer: server authority, replication, every machine sees it | BAR | |
| Q12 | Wildreach is not a music game: no music shown on screen (staff, notes, beat markers, rhythm meters, on-beat prompts, score), no mechanic that needs playing to the music; remove any an earlier pass added | Aaron 11-15 | |
| Q13 | Keep the soundtrack switching smoothly with the situation | Aaron 11-15 | |
| Q14 | Allowed: enemy / boss attack timings lining up with the music; keep any that exist | Aaron 11-15 | |
| Q15 | Git: pull, commit by name, CHANGELOG Unreleased + HANDOFF in the same commit, no release / tag, LFS sizes reported, LANDED line | brief | |
| L1 | Crafting pulls from your chests near the station in your claim (the building cook did it for building) | LATER-COOKS 17 | |
| L2 | A set-complete moment (a chime and a glow on the doll's pieces) when a 4-piece bonus comes on | LATER-COOKS 147 | |
| G1 | Crafting by hand is instant and invisible: no time, no body, no sound but the done chime | grade 2.1 | |
| G2 | The four crafting stations never show their work: a forge glows the same idle at noon as working at night, an order runs with nothing on the anvil, bench or table, finished goods wait invisibly in the tray | grade 2.2 | |
| G3 | Nobody works at a station: you order and the station makes it with no hands; the BAR's "real motion" has nothing here | grade 2.3 | |
| G4 | The forge has no anvil (the AI forge model is a hearth and hood only): a smithy without one | grade 2.2 | |
| G5 | The alchemy table is a tray of bottles lying on the dirt (the model has no legs; it stands 50 cm high) | grade 2.2 | |
| G6 | The charcoal kiln and bloomery breathe no smoke while they work | grade 2.2 | |

## 1. Research

| Game / reference | The specific system | What makes it work | What we take |
|---|---|---|---|
| **Valheim** (workbench, forge, cauldron) | Crafting at a station plays a short work motion: the Viking hammers at the bench with a progress ring; forge upgrades (anvils, cooler, bellows) are physical props placed around the forge, each one widening what it makes | The station is a place you work; its kit is visible in the world | The forge gets its anvil on a stump at its working side; you work at the station with your hands |
| **Palworld** (Primitive Workbench, the "work" verb) | Hold the interact key and your character hammers at the bench; working speeds the build; Pals assigned to the base do it too | Your hands make it go: a reason to stand there, not just a menu | Your hands at the station: +25% speed while you work it; you stop when you walk away and it goes on by itself at its own speed |
| **Red Dead Redemption 2** (crafting at camp / satchel) | Arthur kneels and the item is made in his hands over ~2-3 s: you see the arrows bound, the tonic mixed | Crafting by hand is a body action | Crafting by hand is a timed, committed use: the hero works the piece at the waist, knapping or twisting, with its sound |
| **Kingdom Come: Deliverance II** (Kuttenberg smithy, the smithing minigame) | Heat in the forge with the bellows, hammer on the anvil in time, quench in the trough; a real forge has its anvil a step from the fire, a quench trough beside | Smithing reads as heat → hammer → quench | The forge: the piece heating glows in the coal bed when nobody is at it; on the anvil under the hammer when you are, sparks on each blow; a quench hiss when it's done (we keep no minigame: the timing never asks for skill) |
| **Monster Hunter World** (the Smithy in Astera) | The Provisions / Smithy stations are always alive: the forge breathes, hammers ring, sparks fly | A crafting place is loud and bright when working | Each station sounds like its work (the forge's roar and ring, the plane's hiss, the pestle's grind) and falls quiet idle |
| **V Rising** (refinement stations) | Furnaces and grinders run their queue with the inputs visible, the station animates and glows while working; idle ones go dark | Glanceable state from across the base | Idle forge: banked embers (a low red glow); working: the bed roars and lights the area; the station's state reads from 20 m |
| **Enshrouded** (base crafting) | Stations pull from every chest in the base (magic chests) | No running materials back and forth | Crafting pulls from your chests within 20 m in your claim, bag first, nearest chest next (the building cook's rule) |
| **Conan Exiles** (crafting queue) | The queue runs while you're away; finished goods sit in the station's output | Async crafting | Kept (Raven's queue); the finished goods are now seen on the station |
| **Factorio / Satisfactory / Create** (machines) | Already researched for the machinery lane (`cooks/build-machinery.md` 1.1) | | Graded below, not re-researched |
| **Real world: the smithy** (Hans Sachs's *Ständebuch* woodcuts; Colonial Williamsburg's Anderson blacksmith shop) | The anvil stands one long step from the hearth, horn to the smith's left (a right-handed smith), face at knuckle height (~75 cm), on an oak stump set in the floor bound with iron hoops; the smith holds the work in tongs in the left hand, the hand hammer in the right, ~80-100 blows a minute; quench hiss and steam when done | | Anvil 75 cm face, horn to the smith's left, oak stump with two iron hoops, 1.5 blows a second, tongs in the left hand |
| **Real world: armourers** (Augsburg armourers; Wallace Collection plates) | Plates are raised and planished cold on stakes and blocks with lighter hammers, a fast ringing tap | | The armorer (its model is a block on a stake) taps 2.2 times a second, lighter and higher than the forge |
| **Real world: woodworking** | A hand plane is pushed along the grain in a 0.6-0.8 m stroke, ~0.7 strokes a second, curling shavings off the far end | | The bench: a plane stroke every 1.4 s, shavings curling off |
| **Real world: the apothecary** | A mortar on the bench, the pestle ground round in circles (~0.6 a second); flasks over a spirit lamp steam | | The alchemy table: grinding circles at the mortar, steam over a warmed bowl |
| **Real world: flint knapping** | A hammerstone strikes the core held in the other hand at a steep angle, about two blows a second, flakes falling | | Hand crafting with stone: a hammerstone striking a flint core at 2 blows a second, a click each |

Lessons: (1) a crafting place shows its state at a glance (idle embers, a working roar); (2) the thing being made is
there to see, and so is what's done; (3) your body does the work you order, and your hands make it go; (4) a smithy has
its anvil; (5) materials come from your own stores; (6) every work beat has its own recorded sound.

## 2. Grades of what's built (against THE BAR)

### 2.0 The music rule (Q12-Q14)
Searched the UI and gameplay source for anything that shows the music or asks you to play to it (`WRUI*.cpp`, `WRHUD*`,
`WRPlayerUI.cpp`, creature / boss AI): no staff, notes, beat markers, rhythm meter, on-beat prompt or score. The only
"beat" words are the breath ring's pulse when you're low on air (`WRUIHud.cpp:980`, a heartbeat, not music) and the
creatures' idle behaviour clock (`WRCreature.cpp:3294`). The music director's own bar-aligned switching stays (the
soundtrack, cookpass-audio). Nothing to remove; the crafting work beats added here are work sounds on their own clock,
never tied to the music and never scored.

### 2.1 Crafting by hand (G1): grade D
`FWRCrafting::CraftByHand` (`WRCrafting.cpp:956`) makes the item instantly: inputs gone, item in the bag, the
`craft.done` chime; recipes carry `seconds` (1-4) that nothing reads; usable mid-swing and mid-roll; others see nothing.

### 2.2 Stations (G2, G4, G5, G6): grade C
Forge, armorer, workbench, alchemy table are AI props (Hunyuan, `Content/Props/*`) fitted by `FWRBuildLook::AttachProp`.
Measured from the meshes (exported, 2026-11-16): the **forge** is a stone hearth under a hood and chimney, 115 x 95 x 126
cm as fitted, its coal bed open on its +Y side at 13-20 cm, the flue top at (-9, -9, 126); **no anvil**. The **armorer**
is a block-anvil on a waisted stump on a round stone base, its face at 80 cm (34 x 31 half-size), a stake peg at
(-22, 12). The **workbench** is a trestle bench, top at 81 cm (55 x 33 half-size), a leg vise on its +Y edge, shavings on
its lower shelf. The **alchemy table** is a tray of bottles, bowls and flasks 105 x 95 and 50 cm tall, sitting on the
ground (no legs). The forge's light (`AWRStation::Glow`) burns at full whether it works or not; nothing shows an order
running; the tray's goods are invisible; the kiln and bloomery never smoke. The kitchen stations got this in the
survival pass (`WRKitchenShow.cpp`); the crafting stations did not.

### 2.3 Working (G3): grade D
Ordering is a menu click; the hero never touches the station.

### 2.4 Recipes, queue, tray, Hearth, salvage, blueprints, sets, rings, talismans, runes: grade A-
Raven's queue (`AWRStation::AdvanceQueue`), the build-crafting lane's tiers, salvage with its crimson losses, blueprints,
set bonuses with the doll's set-complete moment (already built: `WRPlayerUI.cpp:1023` plays `gear.setComplete`,
`WRUIInventory.cpp:898` flashes the set's pieces for `ui.setComplete.glowSeconds`; L2 is done), 4-slot armour, rings,
talismans, runes: data-driven, server-checked, mouse and pad. Not raised.

### 2.5 Crafting materials (L1): grade C
Only the bag pays. Building already pays from your chests within 20 m in your claim (`WRBuildStock`, `WRUIBuild.cpp:1618`);
crafting doesn't, so you carry ore from the chest to the forge by hand.

### 2.6 Machinery: grade A-
Power as visible rotation, real drive trains (crank, pitman, cams, bevels), eased speeds, belts with riders, state words
with reasons, sound loops that follow speed, wear and repair, the machine screen with mouse and pad
(`cooks/build-machinery.md` 5). Open items stay on LATER-COOKS (rivers / vents on the map, turrets). Not raised here.

## 3. The raise (design and plan)

### 3.1 Crafting by hand is a timed, committed work (G1)
- **Rule (server, `FWRCraftWork::StartHand`).** A hand recipe (`station: hand`) takes `seconds` x `craft.hand.timeScale`
  (1.0), at least `craft.hand.minSeconds` 0.8. Refused riding, swimming, staggered, frozen, rolling, eating / drinking
  or already working ("already crafting"). Each one: its inputs are taken as it starts (from the bag, then your chests:
  3.4), `State.Drinking` (the item-use tag: no guard, roll or swing; walk x0.4; creatures that read item use read it) for
  its time; at the end, if the tag is still on, it's made (MakeOne: XP, the stack into the bag, the `craft.done` chime);
  if a stagger, a freeze or death took the tag, the inputs come back ("Interrupted: your materials are back"). An order
  of N runs one after another while you have the materials ("craft <id> none N", max `craft.hand.maxBatch` 10).
- **Stop** (`craftstop`): the crafting screen's hand view shows "Making 2 of 5 by hand" with a Stop button (mouse and
  pad) while one runs; the one in your hands finishes, the rest don't start.
- **The body (every machine).** `AWRCharacter::Work` (replicated struct): kind `knap` (stone in the inputs: flint tools,
  throwing stones, spears, scratch stone) or `twist` (everything else: rope, torches, arrows, bait, earplugs, chalk).
  Knap: both forearms forward at the belly, the left hand holds a flint core, the right brings a hammerstone down on it
  2.0 times a second (`craft.work.knap.hz`), a `craft.knap` click and a few stone chips (`craft.chips`) at each blow.
  Twist: both hands together at the belly rolling a hank of fibre, the hands working against each other 0.8 times a
  second, a `gather.fiber` rustle per turn. Held: the core and the hammerstone (`FWRToolMesh` lathe stones, dark grey
  flint with a pale cortex) or the fibre hank (a coiled tube, straw colour). The weapon and the shield / torch are put out
  of sight while the hands are busy and come back after.

### 3.2 The stations show their work (G2, G4, G5, G6)
`AWRStation::TickWorkShow` (new `WRStationShow.cpp`, the kitchen's pattern: built from the replicated queue and tray on
machines that draw, smooth near the camera, a slow check far off, no server cost).

**Forge** (why: heats and forges metal; connects: the coal bed under its hood, the chimney out the top, the anvil a step
from the fire; meets: the anvil's oak stump stands in the ground, never in the forge).
- **The anvil** (new, part of the forge's look, every machine): an oak stump 44 cm across, 50 cm tall, bark on its sides,
  the sawn top showing, two iron hoops; on it a London-pattern anvil modelled in code (`FWRToolMesh`): a waisted body on
  a splayed foot, a 22 x 14 cm face at 76 cm, the horn tapering 18 cm toward the smith's left (+X), the heel with its
  hardy hole toward -X, dark forged iron (0.07). Stands at piece (0, 118) on the forge's open (+Y) side, a 48 cm gap from
  the hearth's face. It collides (a box over stump and anvil, `AnvilBox`), the build ghost shows it.
- **Idle** (no order): banked coals: the hearth light at `craft.show.forge.idle` 0.18 of full, a slow 0.3 Hz breathing.
  A broken Hearth: cold, no light.
- **Working** (an order): the light at full with a fire's flicker (two sines and a gust); `forge.roar` loops (a coal bed
  roaring under the bellows, 3D, 20 m); sparks rise off the bed (`forge.bed` fx: sparks and heat glow); thin smoke from
  the flue top (`kitchen.smoke` at (-9, -9, 128)).
- **The piece being made**: in the coals (0, 6, 20) when nobody works it, glowing (M_Glow, yellow-orange); on the anvil's
  face when the smith works it, cooling from orange to cherry red over each 6 s heat and flaring at each blow. Its shape
  from its output: a blade blank for swords / daggers / greatswords (a tapered bar with a tang), a head for hammers /
  axes / picks (a block), a spearhead, a scythe's curve; a bar (ingot) for ingots and plates; a ring for rings; a cog
  for gears.
- **Done**: what waits in the tray lies on the anvil's face (cooled, its own metal colour, up to 2 pieces) and a quench
  hiss with a puff of steam plays as each finishes (`craft.quench`, `kitchen.steam` burst at the anvil).

**Armorer** (why: plates and leathers are shaped cold; connects: its block and stake; meets: the work lies flat on the
face, off the hardy hole). The piece being made on its face at (12, -14, 81): a dished plate for a chest, a dome for a
head, a cuff for hands, a greave for legs, a hide panel (laced) for hide pieces, a talisman disc on a cord; in its
material (hide brown, copper, bronze, iron, steel, starsteel, the set's own colour for biome sets); a finished piece waits
there. Tapping sound only from a worker (3.3); with nobody, it waits quietly.

**Workbench** (why: wood and tool work; connects: the vise on its +Y edge holds the board; meets: lies on the top). The
work (a plank blank 40 x 12 x 3 cm, or the tool's head and haft laid side by side for tools) on the top at (-14, -6, 82)
along X; finished tools / arrows / bolts lie on the top; curled shavings (`craft.shavings`) fly off the plane stroke.

**Alchemy table** (why: brews; connects: the tray of bottles now stands on a table; meets: its trestle feet on the
ground, the tray on its top). **The trestle** (new, every machine, collides): an oak trestle table 108 x 98 cm, top 4 cm
thick at 70 cm (the tray sits on it), two splayed A-trestles with a stretcher pinned through, wood colour from the
build kit's oak. The prop is lifted 70 cm onto it. **Working**: steam wisps off a warm bowl at (-24, -26, 89)
(`alchemy.steam`), `cook.bubble` simmering; a stone mortar with its pestle stands on the tray's free front strip
(-44, -10, 77). **Done**: finished potions stand on the strip in their vial (FWRFoodLook `bottle`, the potion's colour),
up to 3.

**Charcoal kiln / bloomery** (G6): working, smoke from the kiln's chimney stub and crown vents (`kitchen.smoke` at its
stub, tuning `farm.show.smoke.charcoal_kiln.*`) and from the bloomery's stack top.

### 3.3 Your hands at the station (G3)
- **When**: you order at a forge, armorer, workbench or alchemy table (or open its screen) while the first order in its
  queue is yours and you're not busy (`FWRCraftWork::StartStation`). Kitchen stations, the kiln, the bloomery and
  machines have no hands-on work.
- **The walk in**: your own machine walks the hero to the work spot (real locomotion at a walk, no glide, max 4 m; your
  stick / keys take over and end it), then turns to face the work. Spots (piece frame): forge (0, 178) facing the anvil
  (-Y); armorer (0, -78) facing +Y; workbench (-14, -74) facing +Y; alchemy (-100, 0) facing +X. Not there within
  `craft.work.arriveSeconds` 3 s (blocked by a wall, a chest) → no work ("Stand at the anvil to work it yourself").
- **Worth**: while you're at the spot working, the station runs x`craft.work.handsOn` 1.25 (retimed like power, stacks
  with a drive shaft); its screen says "Your hands at it: +25%".
- **Ends**: you move off the spot (> 60 cm), get staggered / frozen / killed, ride, attack, roll, eat, or the first order
  isn't yours any more (finished, cancelled); the next order yours → you carry on with its item.
- **The body (every machine)**: pose layer `Work` in `FWRHeroAnimProxy::PostLayers` with a phase:
  - forge: right arm raises the hand hammer and brings it down (raise over 0.45 of the cycle, strike in 0.12, rebound),
    1.5 blows a second (`craft.work.forge.hz`); the left forearm forward low holding tongs to the piece; a slight
    knee and spine lean into each blow. At the strike: `craft.anvil` ring (recorded hammer on hot iron), `craft.sparks`
    at the piece, the piece flares.
  - armorer: the same frame, lighter and faster (2.2 Hz, smaller raise), `craft.tap`.
  - workbench: both hands on a plane pushed forward along the board and drawn back (0.7 Hz), the body rocking with it,
    `craft.plane` on each push, shavings at its end.
  - alchemy: the right hand grinds the pestle round the mortar (0.6 Hz circles), the left steadies the mortar,
    `craft.grind` each turn.
  Held: the hand hammer (ash haft, iron head), tongs (two iron reins in the left hand reaching the piece), the plane
  (beech stock, iron, a front knob), the pestle (stone). The weapon and off hand are hidden meanwhile. Beats are on their
  own clock: never the music's (Q12).

### 3.4 Crafting from your chests (L1)
- Recipe inputs (and an infusion's essence) count your bag plus your chests within `building.storageReachCm` 2000 of
  the station (its hand recipes: of you) inside the same claim you can open (`WRBuildStock::InChestsExact`: exact items,
  no planks-for-wood here). Server: Shortfall counts both; TakeInputs takes the bag first, then the nearest chests
  (`WRBuildStock::PayExact`).
- Screen: each cost reads "have 12 (30 in chests)"; the card's shortfall counts both; the max quantity counts both.
- Edge cases: chests in another's claim never pay; a chest emptied between the click and the server is re-checked
  (Shortfall); a cancel's refund goes to the bag (as before).

### 3.5 Data
- `tuning.craft.hand` {timeScale 1, minSeconds 0.8, maxBatch 10}, `craft.work` {handsOn 1.25, reach 60, approachMax 400,
  arriveSeconds 3, knap.hz 2, twist.hz 0.8, forge.hz 1.5, armor.hz 2.2, plane.hz 0.7, pestle.hz 0.6, pose numbers},
  `craft.show` {forge.idle 0.18, forge.full, heatSeconds 6, spots}.
- `sounds.json`: `craft.anvil`, `craft.tap`, `craft.knap`, `craft.plane`, `craft.grind`, `craft.quench`, `forge.roar`
  (loop) from recorded CC0 sources (`Tools/audio/build_craft_audio.py`).
- `fx.json`: `forge.bed` (loop), `craft.sparks`, `craft.chips`, `craft.shavings`, `alchemy.steam` (loop).

### 3.6 Build steps
1. Data rows (tuning, sounds, fx); audio fetched and cut.
2. `WRBuildStock` exact counting and paying; crafting Shortfall / TakeInputs / UI counts (3.4).
3. `FWRCraftWork` (new `WRCraftWork.h/.cpp`): the work rep on the character, hand crafting (3.1), station work (3.3),
   commands `craft` (hand, with N), `craftstop`, `workat`; station speed x1.25.
4. Pose layer `Work` (hero anim), held work tools, beats with sounds and fx (`WRCraftWorkLook.cpp`).
5. `WRStationShow.cpp`: forge anvil + fire states + piece + done goods; armorer; workbench; alchemy trestle + steam +
   mortar + vials; kiln / bloomery smoke; collision for anvil and trestle; ghost parts.
6. Screen: hand progress + Stop, "in chests", hands-on line.
7. CHANGELOG, HANDOFF, compile, spot-check, commit, push, LANDED.

## 4. Spot-check (read against this plan and THE BAR, 2026-11-16)

Fixed while reading back:
- The crafting screen counted your chests for repair and upgrade costs while the server took only the bag: repair and
  upgrade now pay from the bag, then your chests near the station (`WRCrafting.cpp` `WRCraftDetail::PayHere`).
- The UI let a hand batch ask for 50 while the server makes at most 10: the screen's count now stops at
  `craft.hand.maxBatch` (`WRUICrafting.cpp` MaxQuantity).
- The quench steam was first spawned from a looping row (it would never stop): a one-shot `craft.quench` fx row.
- A worker's own pestle and the table's leaning pestle would both show: the leaning one is drawn apart and hidden while
  someone grinds (`WRStationShow.cpp` TickWorkShow).
- Geometry: the smith's spot (0, 178) is 38 cm clear of the anvil block's back face (y 140), the anvil 48 cm clear of the
  hearth's face; the joiner stands 41 cm off the bench edge, the apothecary 46 cm off the table's; the anvil's horn is
  to the smith's left (they face -Y, their left is -X). Nothing is placed through a model.
- Smoothness: the walk in is real locomotion with your own input taking over; the turn eases at 360 deg/s; the pose
  blends in and out over 0.25 s; the piece moves from the coals to the anvil only when the worker arrives.

Known limits: the hero's work poses are procedural arm layers (no captured smithing clips in the hero set), tuned by the
numbers in `craft.work`; Aaron's playtest decides their degrees. Kitchen stations, the kiln, the bloomery and machines
have no hands-on work (by design).

## 5. Ledger proof

| # | Done | Where |
|---|---|---|
| Q1 | Yes | section 1 |
| Q2 | Yes | section 2 (2.1-2.6) |
| Q3 | Yes | section 3, written before the code |
| Q4 | Yes | section 3 built as written (rows below) |
| Q5 | Yes | section 4 |
| Q6 | Yes | this table |
| Q7 | Yes | 3.2 why / connects / meets; spots measured on the exported station meshes (2.2); section 4 geometry |
| Q8 | Yes | `WRCraftWorkLook.cpp:174` walk in and eased turn; pose blend `:141`; heat and flare eased `WRStationShow.cpp` TickWorkShow |
| Q9 | Yes | `Data/tuning.json craft.hand / craft.work / craft.show`; fx rows `Data/fx.json` forge.bed, craft.*; sounds `Data/Audio/sounds.json` craft.*, forge.roar; no damage added |
| Q10 | Yes | Stop is an SWRButton (`WRUICrafting.cpp:812` BuildHandWork), the hands-on line is text; mouse and pad drive both as every crafting button |
| Q11 | Yes | `AWRCharacter::Work` replicated (`WRCharacter.cpp` DOREPLIFETIME), `AWRStation::WorkerRep` replicated; all decisions server (`WRCraftWork.cpp:233`, `:282`, `:327`) |
| Q12 | Yes (nothing to remove) | 2.0: no music UI or music mechanic exists; the work beats run on their own clock |
| Q13 | Yes (kept) | the audio director untouched |
| Q14 | Yes (kept) | nothing touched |
| Q15 | Yes | commit on master, CHANGELOG Unreleased + HANDOFF, LANDED line |
| L1 | Yes | `WRUIBuild.cpp:1645` InChestsExact, `:1652` TakeExact; `WRCrafting.cpp:1034` Have, `:1140` TakeInputs, Shortfall; repair / upgrade `PayHere :993`; screen `WRUICrafting.cpp:501` |
| L2 | Already built (proof) | `WRPlayerUI.cpp:1023` gear.setComplete; `WRUIInventory.cpp:898` the doll's flash |
| G1 | Yes | `WRCraftWork.cpp:233` StartHand, `:127` BeginHandItem, `:75` FinishHandItem, `:58` Refund; `WRPlayerBuild.cpp:1084` craft / craftstop; pose `WRHeroAnimInstance.cpp:507` |
| G2 | Yes | `WRStationShow.cpp:385` TickWorkShow (forge fire states `:403`, piece, done goods `:510`, quench `:534`) |
| G3 | Yes | `WRCraftWork.cpp:282` StartStation, `:327` TickStation; `WRCrafting.cpp:416` x handsOn; `WRCraftWorkLook.cpp:165` tools, beats `:221`; menu `WRPlayerMenus.cpp:346` |
| G4 | Yes | `WRStationShow.cpp:66` Anvil, `:342` BuildWorkFixtures (collision); ghost `WRBuilding.cpp:734` |
| G5 | Yes | `WRStationShow.cpp:89` Trestle; prop lifted (`WRCrafting.cpp` OnRep_Type PropLift / FitLift) |
| G6 | Yes | `WRStationShow.cpp:450` (kiln, bloomery smoke); tuning `farm.show.smoke.charcoal_kiln / bloomery` |
